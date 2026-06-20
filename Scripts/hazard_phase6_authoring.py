# Hazard - Phase 6 editor authoring (headless): emissive flash material for the VFX subsystem.
# Idempotent. The C++ AHazardFlashFX falls back to a default material if this is absent.

import unreal

def L(m): unreal.log("[HAZARD6] " + m)

at = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

full = "/Game/Hazard/Materials/M_FlashFX"
if eal.does_asset_exist(full):
    L("M_FlashFX already exists")
else:
    mat = at.create_asset("M_FlashFX", "/Game/Hazard/Materials", unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    col = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -350, 0)
    col.set_editor_property("parameter_name", "Color")
    col.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    eal.save_asset(full)
    L("Created M_FlashFX (unlit emissive, 'Color' param)")

# Assign it to the existing flash mesh material slot is not needed: AHazardFlashFX picks it up
# via its constructor FObjectFinder on next load.
eal.save_directory("/Game/Hazard", False, True)
L("Phase 6 authoring complete.")
