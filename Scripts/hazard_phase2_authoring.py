# Hazard - Phase 2 editor authoring (headless via UnrealEditor-Cmd -ExecutePythonScript).
# Greybox weapon meshes + weapon BPs, wired onto the player pawn. Idempotent, logged.
# Note: zombie archetypes are data-driven in C++ (one BP_Zombie covers Walker/Runner/Brute),
# and muzzle/hit FX use debug draws this phase (Niagara is a later polish pass).

import unreal

def L(m): unreal.log("[HAZARD2] " + m)
def W(m): unreal.log_warning("[HAZARD2] " + m)

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary

for d in ["/Game/Hazard/Meshes", "/Game/Hazard/Blueprints"]:
    if not eal.does_directory_exist(d):
        eal.make_directory(d)

def dup_mesh(name, src):
    dst = "/Game/Hazard/Meshes/" + name
    if eal.does_asset_exist(dst):
        return unreal.load_asset(dst)
    new = eal.duplicate_asset(src, dst)
    L("Mesh: " + dst) if new else W("dup failed " + dst)
    return new

sm_pistol = dup_mesh("SM_GB_Pistol", "/Engine/BasicShapes/Cube")
sm_blade  = dup_mesh("SM_GB_Blade",  "/Engine/BasicShapes/Cube")

m_grey = unreal.load_asset("/Game/Hazard/Materials/M_Greybox")

def make_bp(name, parent):
    full = "/Game/Hazard/Blueprints/" + name
    if eal.does_asset_exist(full):
        return unreal.load_asset(full)
    f = unreal.BlueprintFactory()
    f.set_editor_property("parent_class", parent)
    bp = asset_tools.create_asset(name, "/Game/Hazard/Blueprints", unreal.Blueprint, f)
    eal.save_asset(full)
    L("Blueprint: " + full)
    return bp

bp_pistol = make_bp("BP_GB_Pistol", unreal.WeaponBase)
bp_blade  = make_bp("BP_GB_Blade",  unreal.WeaponBase)

def cdo_of(bp):
    try:
        return unreal.get_default_object(bp.generated_class())
    except Exception:
        try:
            return bp.generated_class().get_default_object()
        except Exception as ex:
            W("cdo fail %s (%s)" % (bp.get_name(), ex))
            return None

def config_weapon(bp, mode, mesh, scale, extra=None):
    cdo = cdo_of(bp)
    if not cdo:
        return
    try:
        cdo.set_editor_property("mode", mode)
    except Exception as ex:
        W("mode set fail (%s)" % ex)
    try:
        mc = cdo.get_editor_property("mesh_comp")
        if mc:
            if mesh:
                mc.set_editor_property("static_mesh", mesh)
            mc.set_editor_property("relative_scale3d", scale)
            if m_grey:
                mc.set_material(0, m_grey)
    except Exception as ex:
        W("mesh set fail (%s)" % ex)
    if extra:
        for k, v in extra.items():
            try:
                cdo.set_editor_property(k, v)
            except Exception as ex:
                W("prop %s fail (%s)" % (k, ex))
    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    except Exception:
        pass
    eal.save_asset(bp.get_path_name())
    L("Configured " + bp.get_name())

config_weapon(bp_pistol, unreal.WeaponMode.RANGED_HITSCAN, sm_pistol,
              unreal.Vector(0.2, 0.2, 0.5), {"damage": 14.0})
config_weapon(bp_blade, unreal.WeaponMode.MELEE, sm_blade,
              unreal.Vector(0.1, 0.1, 1.2), {"damage": 35.0})

# Wire the weapon classes onto the player pawn's weapon components.
bp_pawn = unreal.load_asset("/Game/Hazard/Blueprints/BP_HazardPlayerPawn")
if bp_pawn:
    pawn_cdo = cdo_of(bp_pawn)
    if pawn_cdo:
        for comp_name, weap_bp in [("right_weapon", bp_pistol), ("left_weapon", bp_blade)]:
            try:
                comp = pawn_cdo.get_editor_property(comp_name)
                if comp and weap_bp:
                    comp.set_editor_property("weapon_class", weap_bp.generated_class())
                    L("Pawn %s -> %s" % (comp_name, weap_bp.get_name()))
            except Exception as ex:
                W("pawn wire %s fail (%s)" % (comp_name, ex))
        try:
            unreal.BlueprintEditorLibrary.compile_blueprint(bp_pawn)
        except Exception:
            pass
        eal.save_asset(bp_pawn.get_path_name())
else:
    W("BP_HazardPlayerPawn not found")

eal.save_directory("/Game/Hazard", False, True)
L("Phase 2 authoring complete.")
