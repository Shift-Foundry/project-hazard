# Hazard - Phase 1 editor authoring (run headless via UnrealEditor-Cmd -ExecutePythonScript).
# Creates greybox materials + meshes, BP data-shell subclasses of the C++ classes, wires the
# GameMode, and builds the playable test level L_HazardArena. Idempotent and heavily logged.

import unreal

def L(msg):  unreal.log("[HAZARD] " + msg)
def W(msg):  unreal.log_warning("[HAZARD] " + msg)
def E(msg):  unreal.log_error("[HAZARD] " + msg)

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary

# ---------------------------------------------------------------- folders
for d in ["/Game/Hazard", "/Game/Hazard/Materials", "/Game/Hazard/Meshes",
          "/Game/Hazard/Blueprints", "/Game/Hazard/Maps"]:
    if not eal.does_directory_exist(d):
        eal.make_directory(d)
L("Folders ready.")

# ---------------------------------------------------------------- materials
def make_material(name, color, roughness=0.85):
    pkg = "/Game/Hazard/Materials"
    full = pkg + "/" + name
    if eal.does_asset_exist(full):
        return unreal.load_asset(full)
    mat = asset_tools.create_asset(name, pkg, unreal.Material, unreal.MaterialFactoryNew())
    mel = unreal.MaterialEditingLibrary
    col = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -350, 0)
    col.set_editor_property("constant", color)
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rgh = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -350, 220)
    rgh.set_editor_property("r", roughness)
    mel.connect_material_property(rgh, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    eal.save_asset(full)
    L("Material created: " + full)
    return mat

m_grey  = make_material("M_Greybox", unreal.LinearColor(0.6, 0.6, 0.6, 1.0))
m_enemy = make_material("M_Greybox_Enemy", unreal.LinearColor(0.70, 0.07, 0.07, 1.0))

# ---------------------------------------------------------------- greybox meshes
def dup_mesh(name, src):
    dst = "/Game/Hazard/Meshes/" + name
    if eal.does_asset_exist(dst):
        return unreal.load_asset(dst)
    new = eal.duplicate_asset(src, dst)
    if new:
        L("Mesh created: " + dst)
    else:
        E("Failed to duplicate " + src + " -> " + dst)
    return new

sm_base   = dup_mesh("SM_GB_Base",   "/Engine/BasicShapes/Cube")
sm_zombie = dup_mesh("SM_GB_Zombie", "/Engine/BasicShapes/Cylinder")
sm_hand   = dup_mesh("SM_GB_Hand",   "/Engine/BasicShapes/Sphere")

# ---------------------------------------------------------------- blueprints
def make_bp(name, parent):
    pkg = "/Game/Hazard/Blueprints"
    full = pkg + "/" + name
    if eal.does_asset_exist(full):
        return unreal.load_asset(full)
    f = unreal.BlueprintFactory()
    f.set_editor_property("parent_class", parent)
    bp = asset_tools.create_asset(name, pkg, unreal.Blueprint, f)
    eal.save_asset(full)
    L("Blueprint created: " + full)
    return bp

bp_zombie = make_bp("BP_Zombie",            unreal.ZombieBase)
bp_base   = make_bp("BP_HazardBase",        unreal.HazardBase)
bp_pawn   = make_bp("BP_HazardPlayerPawn",  unreal.HazardPlayerPawn)
bp_gm     = make_bp("BP_HazardGameMode",    unreal.HazardGameMode)

def cdo_of(bp):
    try:
        return unreal.get_default_object(bp.generated_class())
    except Exception as ex:
        try:
            return bp.generated_class().get_default_object()
        except Exception as ex2:
            E("CDO fetch failed for %s: %s / %s" % (bp.get_name(), ex, ex2))
            return None

def set_component_mesh(bp, comp_name, mesh, mat):
    cdo = cdo_of(bp)
    if not cdo:
        return
    try:
        comp = cdo.get_editor_property(comp_name)
    except Exception as ex:
        W("No property %s on %s (%s)" % (comp_name, bp.get_name(), ex))
        return
    if not comp:
        W("Component %s is null on %s" % (comp_name, bp.get_name()))
        return
    if mesh:
        comp.set_editor_property("static_mesh", mesh)
    if mat:
        try:
            comp.set_material(0, mat)
        except Exception as ex:
            W("set_material failed on %s.%s (%s)" % (bp.get_name(), comp_name, ex))
    L("Assigned mesh/material on %s.%s" % (bp.get_name(), comp_name))

# Greybox slots (these override the engine-shape fallbacks baked into the C++ constructors).
set_component_mesh(bp_zombie, "mesh_comp", sm_zombie, m_enemy)
set_component_mesh(bp_base,   "mesh_comp", sm_base,   m_grey)

# Wire the GameMode BP: enemy = BP_Zombie, pawn = BP_HazardPlayerPawn.
gm_cdo = cdo_of(bp_gm)
if gm_cdo:
    try:
        gm_cdo.set_editor_property("zombie_class", bp_zombie.generated_class())
        gm_cdo.set_editor_property("default_pawn_class", bp_pawn.generated_class())
        L("GameMode wired: ZombieClass=BP_Zombie, DefaultPawn=BP_HazardPlayerPawn")
    except Exception as ex:
        W("GameMode wiring failed (%s) - C++ defaults (AZombieBase/AHazardPlayerPawn) remain." % ex)

# Compile + save the blueprints.
for bp in [bp_zombie, bp_base, bp_pawn, bp_gm]:
    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    except Exception as ex:
        W("compile failed for %s (%s)" % (bp.get_name(), ex))
    eal.save_asset(bp.get_path_name())

# ---------------------------------------------------------------- level
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

map_path = "/Game/Hazard/Maps/L_HazardArena"
L("Creating level " + map_path)
les.new_level(map_path)

# Floor
plane = unreal.load_asset("/Engine/BasicShapes/Plane")
floor = eas.spawn_actor_from_object(plane, unreal.Vector(0, 0, 0))
if floor:
    floor.set_actor_label("Floor")
    floor.set_actor_scale3d(unreal.Vector(60.0, 60.0, 1.0))
    try:
        smc = floor.get_editor_property("static_mesh_component")
        if smc and m_grey:
            smc.set_material(0, m_grey)
    except Exception as ex:
        W("floor material set failed (%s)" % ex)

# Lights (movable so the level is lit without a build)
dlight = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 600), unreal.Rotator(-50.0, 0.0, 30.0))
if dlight:
    try:
        dlight.get_component_by_class(unreal.DirectionalLightComponent).set_mobility(unreal.ComponentMobility.MOVABLE)
    except Exception as ex:
        W("dlight mobility (%s)" % ex)
skylight = eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 400))
if skylight:
    try:
        slc = skylight.get_component_by_class(unreal.SkyLightComponent)
        slc.set_mobility(unreal.ComponentMobility.MOVABLE)
        slc.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
    except Exception as ex:
        W("skylight setup (%s)" % ex)

# Player start (slightly back from the Base, standing height)
eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0.0, -350.0, 90.0))

# Base (BP if available, else the C++ class)
base_class = bp_base.generated_class() if bp_base else unreal.HazardBase
base = eas.spawn_actor_from_class(base_class, unreal.Vector(0.0, 0.0, 0.0))
if base:
    base.set_actor_label("HazardBase")

# World Settings GameMode override -> BP_HazardGameMode (falls back to C++ defaults inside it)
gm_class = bp_gm.generated_class() if bp_gm else unreal.HazardGameMode
world = ues.get_editor_world()
ws = world.get_world_settings()
ws.set_editor_property("default_game_mode", gm_class)
L("World Settings GameMode set to BP_HazardGameMode.")

les.save_current_level()
eal.save_directory("/Game/Hazard", False, True)
L("Phase 1 authoring complete.")
