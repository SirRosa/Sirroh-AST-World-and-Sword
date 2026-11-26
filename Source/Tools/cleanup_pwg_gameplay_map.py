"""Remove only verified legacy showcase actors from L_SirrohGameplay.

Run in Unreal Editor Python or UnrealEditor-Cmd -run=pythonscript.
The original map is backed up locally before any edit. No asset deletion.
"""
import collections
import datetime
import hashlib
import json
from pathlib import Path
import shutil
import unreal

MAP = "/Game/Levels/L_SirrohGameplay"
PROTOTYPE = "/Game/Levels/LevelPrototyping/"
EXPECTED = {
    "/Script/Engine.AmbientSound": 1,
    PROTOTYPE + "Teleporter_Destination.Teleporter_Destination_C": 7,
    PROTOTYPE + "Teleporter_Sender.Teleporter_Sender_C": 15,
    "/Script/Landscape.Landscape": 1,
    PROTOTYPE + "Teleporter_Level.Teleporter_Level_C": 2,
    PROTOTYPE + "SpinningArrow.SpinningArrow_C": 1,
    PROTOTYPE + "LevelButton.LevelButton_C": 6,
    PROTOTYPE + "TargetDummy.TargetDummy_C": 1,
    PROTOTYPE + "LevelBlock.LevelBlock_C": 32,
    PROTOTYPE + "LevelBlock_Traversable.LevelBlock_Traversable_C": 28,
    "/Script/Engine.StaticMeshActor": 1,
}
PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
MAP_FILE = PROJECT / "Content/Levels/L_SirrohGameplay.umap"
REPORT_DIR = PROJECT / "Saved/ChatGPT/SpawnMapCleanup"
REPORT_DIR.mkdir(parents=True, exist_ok=True)
REPORT = REPORT_DIR / "map-cleanup-result.txt"

def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def class_path(actor):
    return actor.get_class().get_path_name()

def inventory(actors):
    return collections.Counter(class_path(a) for a in actors)

def run():
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not level.load_level(MAP):
        raise RuntimeError("Cannot load gameplay map")
    actors = actor_api.get_all_level_actors()
    counts = inventory(actors)
    if counts == {"/Script/Engine.AmbientSound": 1}:
        return {"status": "ALREADY_CLEAN", "remaining": 1, "map_sha256": digest(MAP_FILE)}
    if counts != EXPECTED:
        raise RuntimeError("Actor inventory differs from audited map: " + str(counts))
    for a in actors:
        if not a.get_path_name().startswith(MAP + ".L_SirrohGameplay:PersistentLevel."):
            raise RuntimeError("Unexpected actor outer: " + a.get_path_name())
        if class_path(a) == "/Script/Landscape.Landscape" and a.get_name() != "Landscape_0":
            raise RuntimeError("Unexpected landscape")
        if class_path(a) == "/Script/Engine.StaticMeshActor":
            meshes = a.get_components_by_class(unreal.StaticMeshComponent)
            if a.get_name() != "StaticMeshActor_2" or len(meshes) != 1:
                raise RuntimeError("Unexpected static mesh actor")
            mesh = meshes[0].get_editor_property("static_mesh")
            if not mesh or mesh.get_path_name() != "/Game/Systems/Sprite2Bone/Meshes/Sprite_Plane.Sprite_Plane":
                raise RuntimeError("Unexpected sprite mesh")
    # Do not alter a map with separate external actor/object packages.
    for folder in ("__ExternalActors__", "__ExternalObjects__"):
        p = PROJECT / "Content" / folder / "Levels/L_SirrohGameplay"
        if p.exists() and any(x.is_file() for x in p.rglob("*")):
            raise RuntimeError("External packages require a separate backup: " + str(p))
    stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    backup = REPORT_DIR / ("L_SirrohGameplay.before_" + stamp + ".umap")
    before = digest(MAP_FILE)
    shutil.copy2(MAP_FILE, backup)
    if digest(backup) != before:
        raise RuntimeError("Map backup integrity mismatch")
    doomed = [a for a in actors if class_path(a) != "/Script/Engine.AmbientSound"]
    details = [{"name": a.get_name(), "class": class_path(a)} for a in doomed]
    # Record the backup before attempting edits, including on failure.
    REPORT.write_text(json.dumps({"status":"BACKED_UP","backup":str(backup),"before_sha256":before}, indent=2), encoding="utf-8")
    if len(doomed) != 94 or not actor_api.destroy_actors(doomed):
        raise RuntimeError("Failed to destroy the audited actors; map not saved")
    if inventory(actor_api.get_all_level_actors()) != {"/Script/Engine.AmbientSound": 1}:
        raise RuntimeError("Unexpected remaining actors; map not saved")
    if not level.save_current_level():
        raise RuntimeError("Map save failed")
    after = digest(MAP_FILE)
    if after == before:
        raise RuntimeError("Map was not modified on disk")
    if not level.load_level(MAP):
        raise RuntimeError("Cannot reload saved gameplay map")
    if inventory(actor_api.get_all_level_actors()) != {"/Script/Engine.AmbientSound": 1}:
        raise RuntimeError("Reloaded map inventory mismatch")
    return {"status": "PASS", "removed": len(doomed), "remaining": 1,
            "backup": str(backup), "before_sha256": before, "after_sha256": after,
            "removed_actors": details}

try:
    result = run()
    REPORT.write_text(json.dumps(result, indent=2), encoding="utf-8")
    unreal.log("PWG_MAP_CLEANUP_" + result["status"])
except Exception as exc:
    previous = REPORT.read_text(encoding="utf-8") if REPORT.exists() else ""
    REPORT.write_text(json.dumps({"status":"FAIL","error":str(exc),"previous":previous},indent=2), encoding="utf-8")
    raise
