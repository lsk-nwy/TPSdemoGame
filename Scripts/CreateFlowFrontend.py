"""Run once after compiling: creates only the menu map in this existing project."""
import unreal

path = "/Game/CandyFight/Maps/L_CandyFight_Frontend"
world = unreal.load_asset(path)
if world is None:
    world = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "L_CandyFight_Frontend", "/Game/CandyFight/Maps", unreal.World, unreal.WorldFactory())
if world is None:
    raise RuntimeError("Could not create menu map")
mode = unreal.load_class(None, "/Script/TPSdemo.CandyFightFrontendGameMode")
if mode is None:
    raise RuntimeError("Compile the C++ flow classes before running this script")
world.get_world_settings().set_editor_property("default_game_mode", mode)
if not unreal.EditorAssetLibrary.save_loaded_asset(world, only_if_is_dirty=False):
    raise RuntimeError("Could not save menu map")
unreal.log("[Flow] Frontend map ready: " + path)