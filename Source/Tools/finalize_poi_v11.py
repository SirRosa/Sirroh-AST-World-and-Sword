import unreal,sys,json
from pathlib import Path
root=Path(unreal.Paths.project_dir());sys.path.insert(0,str(root/'Content/Python'))
import poi_authoring as author
for kind in ['Shaft','Surface','Dungeon']:
    table=author.get_catalog(kind)
    assert unreal.EditorAssetLibrary.save_loaded_asset(table,False)
    rows=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    assert not rows, 'Automated tests must leave no user catalog entries'
assert unreal.EditorLoadingAndSavingUtils.load_map(author.ROOT+'/L_POI_Template')
author.preview_art(0)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert unreal.EditorLoadingAndSavingUtils.save_map(world,author.ROOT+'/L_POI_Template')
author.install_menu()
assert unreal.ToolMenus.get().find_menu('LevelEditor.MainMenu.POIAuthoring')
unreal.log('POI_V11_FINAL_ASSETS_OK')
