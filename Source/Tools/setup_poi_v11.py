import unreal, json, sys
from pathlib import Path
root=Path(unreal.Paths.project_dir())
sys.path.insert(0,str(root/'Content/Python'))
import poi_authoring as author
ea=unreal.EditorAssetLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()

def asset(name,folder,cls):
    result=unreal.load_asset(folder+'/'+name)
    if not result:
        factory=unreal.DataAssetFactory(); factory.set_editor_property('data_asset_class',cls)
        result=tools.create_asset(name,folder,cls,factory)
    assert result
    return result

for pool_name,sub,prefix,with_top in [('BG_Bricks','Backgrounds','T_BG_',False),('PF_Planks','Platforms','T_Platform_',True)]:
    pool=asset(pool_name,author.ROOT+'/Pools',unreal.POIArtPool)
    if not pool.images:
        images=[]
        for i in range(1,11):
            pair=unreal.POIImagePair()
            side=unreal.load_asset(author.ROOT+'/Art/'+sub+'/'+prefix+str(i).zfill(2)); assert side
            pair.set_editor_property('side',side)
            if with_top:
                top=unreal.load_asset(author.ROOT+'/Art/Tops/T_Top_'+str(i).zfill(2)); assert top
                pair.set_editor_property('top',top)
            images.append(pair)
        pool.set_editor_property('images',images)
    pool.bake_alpha_bounds()
    assert ea.save_loaded_asset(pool,False)
for layer in range(4):
    theme=asset('DA_Shaft_'+str(layer),author.ROOT+'/Themes',unreal.POIArtTheme)
    if not theme.backgrounds: theme.set_editor_property('backgrounds',unreal.load_asset(author.ROOT+'/Pools/BG_Bricks'))
    if not theme.platforms: theme.set_editor_property('platforms',unreal.load_asset(author.ROOT+'/Pools/PF_Planks'))
    assert ea.save_loaded_asset(theme,False)
table=author.get_catalog(); ea.save_loaded_asset(table,False)
author.install_menu()
# Inspect existing workshop without modifying it. Export validation uses a disposable copy.
source=author.ROOT+'/L_POI_Workshop_AST'
assert unreal.EditorLoadingAndSavingUtils.load_map(source)
all_actors=list(author.actors().get_all_level_actors())
regions=[author.describe_region(a,all_actors) for a in all_actors if author.background(a) and author.eligible(a)]
report={'regions':[{k:v for k,v in r.items() if k!='origin'} for r in regions],
        'pools':{n:len(unreal.load_asset(author.ROOT+'/Pools/'+n).images) for n in ['BG_Bricks','PF_Planks']}}
(root/'Saved/POICheckpoints/Authoring-v11.json').write_text(json.dumps(report,indent=2),encoding='utf8')
unreal.log('POI_V11_SETUP_OK')
