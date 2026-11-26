import unreal
from pathlib import Path
import json

out = Path(unreal.Paths.project_saved_dir()) / 'CarryProps'
out.mkdir(parents=True, exist_ok=True)
paths = [
 '/AST_InventorySystem/Blueprints/Actors/ChildItems/Melee_Weapons/BP_MeleeWeapon',
 '/AST_InventorySystem/Blueprints/Actors/BP_ItemBase',
 '/AST_InventorySystem/Blueprints/Components/BPC_Inventory',
 '/Game/Blueprints/SandboxCharacter_CMC_ABP',
]
for path in paths:
    asset = unreal.load_asset(path)
    if asset:
        data = unreal.BlueprintAnalyzerLibrary.analyze_blueprint(asset)
        (out / (asset.get_name() + '.json')).write_text(unreal.BlueprintAnalyzerLibrary.export_to_json(data), encoding='utf8')
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
matches = []
for asset in registry.get_all_assets():
    name = str(asset.asset_name)
    if any(word in name.lower() for word in ['sword', 'light_template', 'overlay', 'torch']):
        if 'AnimSequence' not in str(asset.asset_class_path):
            matches.append([str(asset.package_name), str(asset.asset_class_path)])
(out / 'assets.json').write_text(json.dumps(matches, indent=2), encoding='utf8')
unreal.log('CARRY_INSPECTION_COMPLETE')
