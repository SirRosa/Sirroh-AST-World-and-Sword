"""Unreal editor Python commandlet. Build once; never overwrites manual edits by default."""
import json, re, math, shutil
from pathlib import Path
import unreal

ROOT = '/Game/Systems/Procedural_Combat'
DEST = ROOT + '/DT_Combat_MotionLibrary'
saved = Path(unreal.Paths.project_saved_dir())
rows = []

def direction(name, hands):
    tail = name.split('Attack_')[-1].replace('p_', '')
    if hands == 2:
        token = re.sub(r'\d+$', '', tail)
        return {'D':(0,-1),'R':(-1,0),'L':(1,0),'RD':(-1,-1),'LD':(1,-1),
                'RU':(-1,1),'LU':(1,1),'RLow':(-1,0),'DoubleRR':(-1,0),'DoubleRL':(-1,0)}.get(token,(0,0))
    for token in tail.split('_'):
        if token == 'T': return (0,-1)
        if token.startswith(('R','L')):
            return (-1 if token.startswith('R') else 1, -1 if 'down' in token or 'dwon' in token else 1 if 'up' in token else 0)
    return (0,0)

def action(name):
    low = name.lower()
    if 'attack' in low:
        for word in ('pommel','kick','halfsword','achilles','whirl','backtrick'):
            if word in low: return word.title()
        return 'Attack'
    if 'guarding_idle' in low or re.fullmatch(r'Longs_Idle[12]', name): return 'Guard'
    if 'taunt' in low: return 'Taunt'
    if 'dodg' in low: return 'Dodge'
    if 'roll' in low: return 'Roll'
    if 'kick' in low: return 'Kick'
    if 'death' in low: return 'Death'
    if 'getup' in low: return 'GetUp'
    if 'knockdown' in low or '_ko_' in low: return 'KO'
    if 'hit' in low: return 'Hit'
    if 'parry' in low: return 'Parry'
    if 'block' in low: return 'Block'
    if 'holster' in low: return 'Holster'
    if 'equip' in low or 'prepare' in low or '2combat' in low: return 'Equip'
    if 'climb' in low: return 'Climb'
    if 'crouch' in low: return 'Crouch'
    if 'sprint' in low: return 'Sprint'
    if 'fall' in low: return 'Fall'
    if 'land' in low: return 'Land'
    if 'jump' in low: return 'Jump'
    if 'turn' in low: return 'Turn'
    if 'walk' in low or 'strafe' in low:
        if 'stop' in low: return 'WalkStop'
        if 'start' in low: return 'WalkStart'
        return 'Walk'
    if 'run' in low: return 'Run'
    if 'skip' in low: return 'Skip'
    return 'Social'

def walk_direction(name):
    low=name.lower()
    angle=0
    if 'bwd' in low: angle=180
    elif 'left' in low: angle=-135 if '135' in low else -45 if '45' in low else -90
    elif 'right' in low: angle=135 if '135' in low else 45 if '45' in low else 90
    return (math.cos(math.radians(angle)), math.sin(math.radians(angle)))

for hands in (1,2):
    folder=ROOT+'/Animations/Combat_%dH_Anim/Animations'%hands
    for path in sorted(unreal.EditorAssetLibrary.list_assets(folder,recursive=True,include_folder=False)):
        seq=unreal.load_asset(path)
        if not isinstance(seq,unreal.AnimSequence): continue
        name=seq.get_name(); low=name.lower(); act=action(name)
        d=direction(name,hands); guard=-1
        if '/AimOffsets/' in path:
            if hands==2:
                code=name.replace('Longs_Look_','')
                d=(-1 if 'L' in code else 1 if 'R' in code else 0,1 if 'U' in code else -1 if 'D' in code else 0)
                act='GuardAim'
            elif 'Blade' in name:
                act='GuardAim'; d=(0,1 if 'High' in name else -1 if 'Low' in name else 0)
            else:
                act='LookAim'; d=(-1 if '_LC' in name else 1 if '_RC' in name else 0,0)
        if act=='Walk':
            d=walk_direction(name)
            guard=0 if 'exploration' in low else 1
        if act=='Guard': guard=1
        intensity=next((v for k,v in [('snap',.1),('fast',.25),('med',.5),('slow',.8),('strong',1)] if k in low),.5)
        thrust='thrust' in low or 'point' in low
        moving=('move_' in low or (hands==2 and act=='Attack' and '_p_' not in low))
        row={'Name':str(hands)+'H_'+name,'Animation':seq.get_path_name(),'Hands':hands,'Action':act,
            'bEnabled':True,'bMoving':moving,'Direction':{'X':d[0],'Y':d[1]},'Intensity':intensity,
            'Height':-1 if 'low' in low else 1 if 'high' in low or '_hi' in low else 0,
            'Variant':int(re.search(r'(\d+)$',name).group(1)) if re.search(r'(\d+)$',name) else 0,
            'Airborne':1 if act=='Attack' and 'jump' in low else 0,
            'Sprint':1 if act=='Attack' and '_run_' in low else -1,'Crouch':-1,'Guard':guard,
            'bCuts':act=='Attack' and not thrust,'bThrusts':act=='Attack' and thrust,
            'NativeFamily':'Pierce' if thrust else 'Light','LoadTime':.2,'ContactTime':.55,'FinishTime':1.,
            'PoseWeight':1.,'AdaptationDegrees':40.,'NeutralTarget':{'X':100,'Y':0,'Z':0}}
        if act=='Guard': row['Priority']=1 if name.endswith(('Idle1','Idle_1')) else 0
        if 'Guarding_3_Walk_Loop' in name: row['Priority']=.1
        # Explicit editable phase estimates, not fabricated animation notifies.
        if act=='Attack' and ('double' in low or '2xattack' in low):
            row['StrikeWindows']=[{'X':.2,'Y':.48},{'X':.55,'Y':.92}]
            row['StrikeDirections']=[{'X':d[0],'Y':d[1]},{'X':-d[0] if 'rl' in low else d[0],'Y':d[1]}]
        rows.append(row)

table=unreal.load_asset(DEST) if unreal.EditorAssetLibrary.does_asset_exist(DEST) else None
if table:
    # Idempotent: retain all existing fields, including user-disabled rows.
    existing=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    names={r['Name'] for r in existing}
    rows=existing+[r for r in rows if r['Name'] not in names]
else:
    factory=unreal.DataTableFactory()
    factory.set_editor_property('struct',unreal.load_object(None,'/Script/Sirroh_Reimagineg_v3.CombatMotionSource'))
    table=unreal.AssetToolsHelpers.get_asset_tools().create_asset('DT_Combat_MotionLibrary',ROOT,unreal.DataTable,factory)
if not table: raise RuntimeError('Cannot create library DataTable')
before=unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table)
(saved/'CombatV3-Library-before.json').write_text(before,encoding='utf-8')
if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(rows)):
    raise RuntimeError('Library import failed')
back=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
if len(back)!=len(rows): raise RuntimeError('Library row count mismatch')
if not unreal.EditorAssetLibrary.save_loaded_asset(table,False): raise RuntimeError('Cannot save library')
styles=unreal.load_asset(ROOT+'/DT_Combat_Styles')
original=unreal.DataTableFunctionLibrary.export_data_table_to_json_string(styles)
(saved/'CombatV3-Styles-before-Library.json').write_text(original,encoding='utf-8')
asset=Path(unreal.Paths.project_content_dir())/'Systems/Procedural_Combat/DT_Combat_Styles.uasset'
backup=saved/'DT_Combat_Styles.pre-Library.uasset'
if not backup.exists(): shutil.copy2(asset,backup)
style_rows=json.loads(original)
for row in style_rows:
    if row['Name']=='Gaidheal': row['MotionLibrary']=table.get_path_name(); row['MotionChooser']='None'
if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(styles,json.dumps(style_rows)):
    raise RuntimeError('Style import failed')
check=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(styles))
if not any(r['Name']=='Gaidheal' and r['MotionLibrary']==table.get_path_name() for r in check):
    raise RuntimeError('Gaidheal library binding missing')
if not unreal.EditorAssetLibrary.save_loaded_asset(styles,False): raise RuntimeError('Cannot save styles')
(saved/'CombatV3-Library.json').write_text(json.dumps(back,indent=2),encoding='utf-8')
unreal.log('COMBAT_LIBRARY_SAVED rows=%d'%len(back))
