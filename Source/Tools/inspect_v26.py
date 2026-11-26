import unreal
from pathlib import Path
bp=unreal.load_asset('/AST_InventorySystem/UI/WeaponHUD')
(Path(unreal.Paths.project_saved_dir())/'POICheckpoints/WeaponHUD-v26.json').write_text(unreal.BlueprintAnalyzerLibrary.export_to_json(unreal.BlueprintAnalyzerLibrary.analyze_blueprint(bp)),encoding='utf8')
