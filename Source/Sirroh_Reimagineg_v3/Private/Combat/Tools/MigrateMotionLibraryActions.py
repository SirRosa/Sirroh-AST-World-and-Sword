"""Explicit migration of classifications corrected by the project author."""
import unreal,json,copy,math
from pathlib import Path

path='/Game/Systems/Procedural_Combat/DT_Combat_MotionLibrary'
table=unreal.load_asset(path)
text=unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table)
saved=Path(unreal.Paths.project_saved_dir())
backup=saved/'CombatV3-Library-before-actions.json'
if not backup.exists(): backup.write_text(text,encoding='utf-8')
rows=json.loads(text)
for r in rows:
    name=r['Name']; clip=name.split('H_',1)[-1]
    if name.startswith('1H_'):
        if clip in ('Sword1h_Idle','Sword1h_Idle_1','Sword1h_Idle2','Sword1h_Idle3'):
            r.update(Action='Guard',Guard=1,Priority=2 if clip=='Sword1h_Idle' else 1,bCuts=False,bThrusts=False)
        elif clip.startswith('Sword1h_Guarding_Idle') or clip=='Sword1h_Relaxed_Idle':
            r.update(Action='Carry',Guard=0,Priority=1 if clip=='Sword1h_Guarding_Idle_1' else 0)
        elif r['Action']=='Walk' and 'Guarding' in clip:
            r.update(Action='CarryWalk',Guard=0)
        if clip=='Sword1h_Equip': r.update(Action='Equip',Priority=3)
        if 'ShortDodge' in clip: r['Action']='ShortDodge'
    if 'Exploration' in clip and r['Action']=='Walk': r.update(Action='CarryWalk',Guard=0)
    if r['Action'] in ('Dodge','Roll'):
        x,y=(0,-1) if 'Left' in clip else (0,1) if 'Right' in clip else (-1,0) if 'Bwd' in clip else (1,0)
        r['Direction']={'X':x,'Y':y}
        r['Priority']=-2 if clip=='Sword1h_Dodge' else 0
        if r['Action']=='Roll': r['Hands']=0 # shared reference, no duplicated animation assets
    if r['Action']=='Kick':
        r['LoadTime']=.2; r['ContactTime']=.75
        r['bMoving']='Move_' in clip
    if name.startswith('2H_') and 'Sprint' in clip:
        r['Hands']=0
        r['Priority']=1 if 'Loop' in clip else -1
    if 'Crouch' in clip:
        r['Action']='CrouchStart' if 'Start' in clip else 'CrouchEnd' if 'End' in clip else 'Crouch'
    if r['Action']=='Land': r['Priority']=1 if clip.endswith('_Land') else -1
    if r['Action']=='Fall': r['Priority']=1 if 'FallingLoop' in clip else 0
    if r['Action']=='Turn':
        r['Direction']={'X':0,'Y':-1 if 'TurnL' in clip else 1}
        r['Priority']=1 if '180' in clip else 0
    if r['Action']=='Run':
        angle=180 if 'Bwd' in clip else 0
        if 'Left' in clip or '_Lt' in clip: angle=-135 if '135' in clip else -45 if '45' in clip else -90
        if 'Right' in clip or '_Rt' in clip: angle=135 if '135' in clip else 45 if '45' in clip else 90
        r['Direction']={'X':math.cos(math.radians(angle)),'Y':math.sin(math.radians(angle))}
    if r['Action']=='Carry' or r['Action']=='CarryWalk': r['bCuts']=False; r['bThrusts']=False
if not any(r['Name']=='2H_Carry_Idle' for r in rows):
    r=copy.deepcopy(next(r for r in rows if r['Name']=='2H_Longs_Idle1'))
    r.update(Name='2H_Carry_Idle',Action='Carry',Guard=0,Priority=1)
    rows.append(r)
if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(rows)): raise RuntimeError('Import failed')
back=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
if len(back)!=len(rows): raise RuntimeError('Readback row count mismatch')
for r in back:
    if r['Action']=='Guard' and r['Hands']==1 and 'Guarding' in r['Name']: raise RuntimeError('Wrong 1H guard classification')
if not unreal.EditorAssetLibrary.save_loaded_asset(table,False): raise RuntimeError('Save failed')
unreal.log('COMBAT_ACTIONS_MIGRATED rows=%d guard1H=%s'%(len(back),','.join(r['Name'] for r in back if r['Action']=='Guard' and r['Hands']==1)))
