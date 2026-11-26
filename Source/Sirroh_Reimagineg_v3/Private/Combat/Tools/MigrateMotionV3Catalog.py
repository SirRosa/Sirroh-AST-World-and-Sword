"""Editor commandlet: lossless catalog migration. No legacy package is modified.

Defaults to dry-run. Set COMBAT_CATALOG_WRITE=1 to write after validation.
Existing manually edited catalogs are never overwritten; conflicts stop the migration.
"""
import copy
import json
import math
import os
import re
import shutil
from pathlib import Path
import unreal

ROOT = '/Game/Systems/Procedural_Combat'
DEST = ROOT + '/DT_Combat_Motion_v3'
OUT = Path(unreal.Paths.project_saved_dir()) / 'CombatV3-CatalogMigration'
OUT.mkdir(exist_ok=True)
WRITE = os.environ.get('COMBAT_CATALOG_WRITE') == '1'


def comparable(value):
    """Package save assigns FText namespace/key; compare the display text, not generated IDs."""
    if isinstance(value, dict):
        result = {k: comparable(v) for k, v in value.items()}
        text = result.get('DisplayName')
        if isinstance(text, str) and text.startswith('NSLOCTEXT('):
            parts = re.findall(r'"((?:\\.|[^"\\])*)"', text)
            if len(parts) == 3:
                result['DisplayName'] = parts[2]
        return result
    if isinstance(value, list):
        return [comparable(v) for v in value]
    return value


def read_table(name):
    table = unreal.load_asset(ROOT + '/' + name)
    if not table:
        raise RuntimeError('Missing source table: ' + name)
    rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    backup = OUT / (name + '.before.json')
    if not backup.exists():
        backup.write_text(json.dumps(rows, indent=2), encoding='utf-8')
        source = Path(unreal.Paths.project_content_dir()) / ('Systems/Procedural_Combat/' + name + '.uasset')
        shutil.copy2(source, OUT / (name + '.before.uasset'))
    return rows


library = read_table('DT_Combat_MotionLibrary')
styles = read_table('DT_Combat_Styles')
effects = read_table('DT_Combat_Effects')
profiles = {}
groups = {}
manifest = []
warnings = []


def profile(name, parent='None'):
    row = {'Name': name, 'ParentProfile': parent, 'Revision': 1}
    profiles[name] = row
    return row


shared = profile('Shared')
shared['Effects'] = {'Mode': 'Override', **{r['Name']: {k: v for k, v in r.items() if k != 'Name'} for r in effects}}
for hands in (1, 2):
    row = profile('Base_%dH' % hands, 'Shared')
    row['Style'] = {'bTwoHanded': hands == 2}
    row['OverrideStyleFields'] = ['bTwoHanded']


def direction(x, y):
    if x*x + y*y < .001:
        return 'Mid'
    index = round(math.atan2(x, y) / (math.pi / 4)) % 8
    return ['Top', 'TopRight', 'Right', 'LowRight', 'Bottom', 'LowLeft', 'Left', 'TopLeft'][index]


def classify(m):
    action = m['Action']
    if action in ('GuardAim', 'LookAim'):
        d = m['Direction']
        return action, 'Aim_%g_%g' % (d['X'], d['Y']), 'InPlace'
    d = m.get('Direction', {'X': 0, 'Y': 0})
    slot = direction(d['X'], d['Y'])
    context = 'Moving' if m.get('bMoving') else 'InPlace'
    name = m.get('Name', '')
    if action == 'Attack':
        category = 'Offense.InAir' if m.get('Airborne') == 1 else 'Offense.Pierce' if m.get('bThrusts') else 'Offense.Heavy' if m.get('Intensity', .5) >= .65 else 'Offense.Light'
        return category, slot, context
    if action in ('Block', 'Parry'):
        clip = name.split('H_', 1)[-1]
        if clip.startswith('Longs_Block') and not clip.startswith('Longs_Block_'):
            return 'Other.Guarding', 'Mid', clip.replace('Longs_Block', '') or 'Loop'
        if clip.startswith('Sword1h_Parry_'):
            tail = clip.replace('Sword1h_Parry_', '')
            slot = {'T': 'Top', 'R': 'Right', 'Rd': 'LowRight', 'LowRight': 'LowRight', 'L': 'Left', 'L2': 'Left', 'Mid': 'Mid', 'Mid2': 'Mid'}[tail]
        else:
            tail = clip.replace('Longs_Block_', '')
            context = 'InPlace' if tail.startswith('p_') else 'Moving'
            tail = tail.removeprefix('p_')
            # Top established from actual socket/pose samples. Side labels retained
            # provisionally, separately from runtime defense until its visual gate.
            slot = {'D': 'Top', 'L': 'Left', 'L2': 'Left', 'R': 'Right', 'R2': 'TopLeft', 'LLow': 'LowLeft', 'RLow': 'LowRight'}.get(tail, 'Mid')
            if tail != 'D':
                warnings.append(name + ': side defense sector provisional; verify at defense animation stage')
        return 'Received.Block', slot, context
    mapping = {
        'Guard': 'Movement.Combat.Base', 'Carry': 'Movement.OutOfCombat.Base',
        'Walk': 'Movement.Combat.Walk', 'CarryWalk': 'Movement.OutOfCombat.Walk',
        'Equip': 'Movement.OutOfCombat.Transition', 'Holster': 'Movement.Combat.Transition',
        'Dodge': 'Movement.Dodge', 'Roll': 'Movement.Roll', 'Taunt': 'Other.Taunt',
        'Kick': 'Offense.Kick', 'Hit': 'Received.HitReaction', 'KO': 'Received.Knockdown',
        'Death': 'Received.Death', 'GetUp': 'Received.GetUp', 'Air': 'Received.InAir', 'Ground': 'Received.OnGround'
    }
    if action in mapping:
        return mapping[action], slot, context
    if action in ('Pommel', 'Halfsword', 'Achilles', 'Whirl', 'Backtrick'):
        return 'Offense.Specials.' + action, slot, context
    return ('Other.' if action == 'Social' else 'Movement.') + action, slot, context


def add(owner, category, slot, context, region, variant, provenance):
    key = (owner, category, slot, context, region)
    groups.setdefault(key, []).append(variant)
    manifest.append({'source': provenance, 'profile': owner, 'category': category, 'direction': slot,
                     'context': context, 'region': region, 'variant': variant['Id'], 'animation': variant['Motion']['Animation']})


# Parse the current explicit reaction catalog, preserving every selector flag.
source = Path(unreal.Paths.project_dir()) / 'Source/Sirroh_Reimagineg_v3/Private/Combat/CombatReactionCatalog.cpp'
pattern = r'\{TEXT\("([^"]+)"\),TEXT\("([^"]+)"\),TEXT\("([^"]+)"\),TEXT\("([^"]+)"\),FVector\(([^)]+)\),(\d+),(true|false),(true|false),(true|false),(true|false)\}'
reactions = []
for match in re.finditer(pattern, source.read_text(encoding='utf-8-sig')):
    rid, path, action, region, xyz, hands, strong, turn, pierce, cut = match.groups()
    reactions.append({'Id': rid, 'Path': path, 'Action': action, 'Region': region, 'Hands': int(hands),
                      'Incoming': dict(zip(('X', 'Y', 'Z'), map(float, xyz.split(',')))),
                      'bStrong': strong == 'true', 'bTurn': turn == 'true', 'bPierce': pierce == 'true', 'bCut': cut == 'true'})
if not reactions:
    raise RuntimeError('Reaction catalog parse failed; refusing incomplete migration')
used_reactions = set()
for raw in library:
    m = copy.deepcopy(raw)
    category, slot, context = classify(m)
    rid = m.pop('Name')
    owner = 'Base_%dH' % m['Hands'] if m['Hands'] else 'Shared'
    variant = {'Id': rid, 'Motion': m}
    region = 'Any'
    reaction = next((r for r in reactions if r['Path'] == m['Animation'].split('.')[0] and r['Action'] == m['Action']), None)
    if reaction:
        used_reactions.add(reaction['Id'])
        region = reaction['Region']
        variant.update({k: reaction[k] for k in ('Incoming', 'bStrong', 'bTurn', 'bPierce', 'bCut')})
        variant['LegacyReactionId'] = reaction['Id']
    add(owner, category, slot, context, region, variant, 'MotionLibrary/' + rid)

for r in reactions:
    if r['Id'] in used_reactions:
        continue
    a = unreal.load_asset(r['Path'])
    if not isinstance(a, unreal.AnimSequence):
        raise RuntimeError('Missing reaction animation: ' + r['Path'])
    m = {'Animation': a.get_path_name(), 'Action': r['Action'], 'Hands': r['Hands'],
         'bCuts': False, 'bThrusts': False, 'Airborne': 1 if r['Action'] == 'Air' else 0}
    category, _, context = classify(m)
    incoming = r['Incoming']
    slot = direction(incoming['Y'], incoming['Z']) if incoming['Y'] or incoming['Z'] else 'Front' if incoming['X'] >= 0 else 'Back'
    variant = {'Id': 'Reaction_' + r['Id'], 'Motion': m, 'LegacyReactionId': r['Id'],
               **{k: r[k] for k in ('Incoming', 'bStrong', 'bTurn', 'bPierce', 'bCut')}}
    owner = 'Base_%dH' % r['Hands'] if r['Hands'] else 'Shared'
    add(owner, category, slot, context, r['Region'], variant, 'ReactionCatalog/' + r['Id'])

# Keep previously inactive legacy clips, with source metadata, without activating them.
known = {entry['animation'] for entry in manifest}
for style in styles:
    for hands in (1, 2):
        name = style['Name'] + '_%dH' % hands
        row = profile(name, 'Base_%dH' % hands)
        row['bSelectable'] = hands == 2
        row['OtherGripProfile'] = style['Name'] + ('_2H' if hands == 1 else '_1H')
        row['Style'] = copy.deepcopy({k: v for k, v in style.items() if k != 'Name'})
        row['Style']['bTwoHanded'] = hands == 2
        row['Style']['MotionLibrary'] = 'None'
        row['OverrideStyleFields'] = list(row['Style'])
    for field in ('MotionSources', 'ThrustMotionSources'):
        for index, original in enumerate(style.get(field, [])):
            if original['Animation'] in known:
                continue
            # Inline banks are inactive while the old style has MotionLibrary assigned.
            m = copy.deepcopy(original)
            variant = {'Id': 'Legacy_%s_%s_%d' % (style['Name'], field, index), 'Motion': m}
            add('Base_2H', 'Other.Archive', 'Mid', 'Archived', 'Any', variant, 'Styles/' + style['Name'] + '/' + field)
            known.add(m['Animation'])

inventory = []
for path in sorted(unreal.EditorAssetLibrary.list_assets(ROOT + '/Animations', recursive=True, include_folder=False)):
    a = unreal.load_asset(path)
    if not isinstance(a, unreal.AnimSequence):
        continue
    inventory.append(a.get_path_name())
    if a.get_path_name() in known:
        continue
    m = {'Animation': a.get_path_name(), 'Action': 'Archived', 'Hands': 2 if '/LongSword/' in path else 0,
         'bEnabled': False, 'bCuts': False, 'bThrusts': False}
    add('Base_2H' if m['Hands'] else 'Shared', 'Other.Archive', 'Mid', 'Archived', 'Any',
        {'Id': 'Asset_' + a.get_name(), 'Motion': m}, 'AssetInventory/' + a.get_name())


def get_category(row, path):
    parts = path.split('.')
    if parts[0] == 'Offense' and parts[1] == 'Specials':
        container = row.setdefault('Offense', {}).setdefault('Specials', [])
        name = parts[2]
    elif len(parts) == 2 and ((parts[0] == 'Movement' and parts[1] not in ('Roll', 'Dodge')) or (parts[0] == 'Other' and parts[1] not in ('Taunt', 'Guarding'))):
        container = row.setdefault(parts[0], {}).setdefault('Additional', [])
        name = parts[1]
    else:
        node = row
        for part in parts:
            node = node.setdefault(part, {})
        return node
    entry = next((e for e in container if e['Name'] == name), None)
    if entry is None:
        entry = {'Name': name, 'Data': {}}
        container.append(entry)
    return entry['Data']


for (owner, category, slot, context, region), variants in sorted(groups.items()):
    data = get_category(profiles[owner], category)
    data['Mode'] = 'Override'
    variants.sort(key=lambda v: v['Id'])
    sets = [{'Name': 'Variants_%02d_%02d' % (start+1, min(start+10, len(variants))), 'Variants': variants[start:start+10]}
            for start in range(0, len(variants), 10)]
    data.setdefault('Slots', []).append({'Direction': slot, 'Context': context, 'Region': region, 'Mode': 'Override', 'Sets': sets})

for name in ('Base_1H', 'Base_2H'):
    for target in ('Offense.Light', 'Offense.Heavy', 'Offense.Pierce'):
        get_category(profiles[name], target).update(Mode='Override', bOverrideLayering=True, bUseLayeredAnimation=True)
    for target, other in (('Offense.Heavy', 'Offense.Light'), ('Received.PerfectBlock', 'Received.Block'), ('Received.Parry', 'Received.Block')):
        data = get_category(profiles[name], target)
        data.update(Mode='Override', bOverrideReuse=True, UseOtherAnimations={'Category': other})

rows = list(profiles.values())
expected = set(inventory) | {unreal.load_asset(r['Path']).get_path_name() for r in reactions} | {r['Animation'] for r in library}
actual = {entry['animation'] for entry in manifest}
if expected - actual:
    raise RuntimeError('Unmigrated references: ' + str(sorted(expected - actual)))
report = {'profiles': len(rows), 'source_rows': len(library), 'reaction_entries': len(reactions),
          'unique_animations': len(actual), 'missing': sorted(expected - actual), 'warnings': warnings, 'entries': manifest}
(OUT / 'manifest.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
(OUT / 'proposed.json').write_text(json.dumps(rows, indent=2), encoding='utf-8')
if not WRITE:
    unreal.log('CATALOG_DRY_RUN profiles=%d animations=%d missing=0' % (len(rows), len(actual)))
else:
    table = unreal.load_asset(DEST) if unreal.EditorAssetLibrary.does_asset_exist(DEST) else None
    baseline_path = OUT / 'imported-baseline.json'
    if table:
        current = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
        if not baseline_path.exists() or comparable(current) != comparable(json.loads(baseline_path.read_text(encoding='utf-8'))):
            raise RuntimeError('Catalog contains manual edits or lacks baseline. Proposed migration saved; existing table untouched.')
    else:
        factory = unreal.DataTableFactory()
        factory.set_editor_property('struct', unreal.load_object(None, '/Script/Sirroh_Reimagineg_v3.CombatMotionProfile'))
        table = unreal.AssetToolsHelpers.get_asset_tools().create_asset('DT_Combat_Motion_v3', ROOT, unreal.DataTable, factory)
    if not table or not unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows)):
        raise RuntimeError('New catalog JSON import failed')
    # Unreal Python consumes a bool return as success and returns the out parameter.
    errors = unreal.CombatMotionCatalog.validate_catalog(table)
    if errors is None or len(errors):
        raise RuntimeError('Catalog validation failed: ' + str(errors))
    back = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    # Readback scans serialized variant entries, not unrelated metadata strings.
    def animations(value):
        if isinstance(value, dict):
            result = {value['Motion']['Animation']} if 'Motion' in value and 'Id' in value else set()
            for child in value.values():
                result |= animations(child)
            return result
        if isinstance(value, list):
            return set().union(*(animations(child) for child in value)) if value else set()
        return set()
    if actual != animations(back) or len(back) != len(rows):
        raise RuntimeError('Readback completeness mismatch; catalog not saved')
    if not unreal.EditorAssetLibrary.save_loaded_asset(table, False):
        raise RuntimeError('Catalog save failed')
    back = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    baseline_path.write_text(json.dumps(back, indent=2), encoding='utf-8')
    unreal.log('CATALOG_SAVED profiles=%d animations=%d missing=0' % (len(back), len(actual)))
