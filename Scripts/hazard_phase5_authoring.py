# Hazard - Phase 5 editor authoring (headless): main-menu level + PC preview room level.
# Idempotent, logged.

import unreal

def L(m): unreal.log("[HAZARD5] " + m)
def W(m): unreal.log_warning("[HAZARD5] " + m)

eal = unreal.EditorAssetLibrary
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

if not eal.does_directory_exist("/Game/Hazard/Maps"):
    eal.make_directory("/Game/Hazard/Maps")

m_grey = unreal.load_asset("/Game/Hazard/Materials/M_Greybox")
plane = unreal.load_asset("/Engine/BasicShapes/Plane")
cube = unreal.load_asset("/Engine/BasicShapes/Cube")

def add_light(loc, rot):
    dl = eas.spawn_actor_from_class(unreal.DirectionalLight, loc, rot)
    if dl:
        try:
            dl.get_component_by_class(unreal.DirectionalLightComponent).set_mobility(unreal.ComponentMobility.MOVABLE)
        except Exception as ex:
            W("dlight %s" % ex)
    sl = eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 400))
    if sl:
        try:
            slc = sl.get_component_by_class(unreal.SkyLightComponent)
            slc.set_mobility(unreal.ComponentMobility.MOVABLE)
            slc.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
        except Exception as ex:
            W("skylight %s" % ex)

def floor(scale):
    f = eas.spawn_actor_from_object(plane, unreal.Vector(0, 0, 0))
    if f:
        f.set_actor_label("Floor")
        f.set_actor_scale3d(unreal.Vector(scale, scale, 1.0))
        try:
            smc = f.get_editor_property("static_mesh_component")
            if smc and m_grey:
                smc.set_material(0, m_grey)
        except Exception:
            pass
    return f

def wall(name, loc, scale):
    w = eas.spawn_actor_from_object(cube, loc)
    if w:
        w.set_actor_label(name)
        w.set_actor_scale3d(scale)
        try:
            smc = w.get_editor_property("static_mesh_component")
            if smc and m_grey:
                smc.set_material(0, m_grey)
        except Exception:
            pass
    return w

def set_gm(gm_class):
    world = ues.get_editor_world()
    ws = world.get_world_settings()
    ws.set_editor_property("default_game_mode", gm_class)

# ---------------- Main menu level ----------------
L("Creating L_MainMenu")
les.new_level("/Game/Hazard/Maps/L_MainMenu")
add_light(unreal.Vector(0, 0, 600), unreal.Rotator(-50.0, 0.0, 30.0))
set_gm(unreal.HazardMenuGameMode)
les.save_current_level()

# ---------------- PC preview room ----------------
L("Creating L_PreviewRoom")
les.new_level("/Game/Hazard/Maps/L_PreviewRoom")
floor(18.0)  # ~18m floor
add_light(unreal.Vector(0, 0, 600), unreal.Rotator(-50.0, 0.0, 30.0))

# Enclosing greybox walls (~ +/- 850cm, 4m tall) — mock "scanned room".
R = 850.0
H = 200.0
T = 0.2  # thin
wall("Wall_N", unreal.Vector(0, R, H),  unreal.Vector(17.0, T, 4.0))
wall("Wall_S", unreal.Vector(0, -R, H), unreal.Vector(17.0, T, 4.0))
wall("Wall_E", unreal.Vector(R, 0, H),  unreal.Vector(T, 17.0, 4.0))
wall("Wall_W", unreal.Vector(-R, 0, H), unreal.Vector(T, 17.0, 4.0))

eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0.0, -350.0, 90.0))

bp_base = unreal.load_asset("/Game/Hazard/Blueprints/BP_HazardBase")
base_class = bp_base.generated_class() if bp_base else unreal.HazardBase
b = eas.spawn_actor_from_class(base_class, unreal.Vector(0.0, 0.0, 0.0))
if b:
    b.set_actor_label("HazardBase")

bp_gm = unreal.load_asset("/Game/Hazard/Blueprints/BP_HazardGameMode")
gm_class = bp_gm.generated_class() if bp_gm else unreal.HazardGameMode
set_gm(gm_class)
les.save_current_level()

L("Phase 5 authoring complete.")
