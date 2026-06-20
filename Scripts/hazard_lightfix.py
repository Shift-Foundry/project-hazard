# Hazard - fix dark levels: proper ambient (SkyAtmosphere + real-time SkyLight + brighter sun)
# and a faint emissive on the greybox material so geometry is always visible. Idempotent.

import unreal

def L(m): unreal.log("[HAZARD_LIGHT] " + m)
def W(m): unreal.log_warning("[HAZARD_LIGHT] " + m)

eal = unreal.EditorAssetLibrary
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

def fix_level(path):
    if not les.load_level(path):
        W("could not load " + path); return
    actors = eas.get_all_level_actors()
    has_atmo = False
    for a in actors:
        if isinstance(a, unreal.DirectionalLight):
            c = a.get_component_by_class(unreal.DirectionalLightComponent)
            if c:
                c.set_mobility(unreal.ComponentMobility.MOVABLE)
                c.set_editor_property("intensity", 6.0)
                try:
                    c.set_editor_property("atmosphere_sun_light", True)
                except Exception as ex:
                    W("sun flag: %s" % ex)
        elif isinstance(a, unreal.SkyLight):
            c = a.get_component_by_class(unreal.SkyLightComponent)
            if c:
                c.set_mobility(unreal.ComponentMobility.MOVABLE)
                try:
                    c.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_CAPTURED_SCENE)
                    c.set_editor_property("real_time_capture", True)
                    c.set_editor_property("intensity_scale", 1.5)
                except Exception as ex:
                    W("skylight props: %s" % ex)
        elif isinstance(a, unreal.SkyAtmosphere):
            has_atmo = True
    if not has_atmo:
        eas.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
        L("added SkyAtmosphere to " + path)
    les.save_current_level()
    L("lit " + path)

for p in ["/Game/Hazard/Maps/L_PreviewRoom", "/Game/Hazard/Maps/L_HazardArena"]:
    fix_level(p)

# Faint emissive on the greybox material (guaranteed visibility even in shadow).
mel = unreal.MaterialEditingLibrary
def add_emissive(mat_path, color):
    mat = unreal.load_asset(mat_path)
    if not mat:
        W("no material " + mat_path); return
    em = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -350, 450)
    em.set_editor_property("constant", color)
    mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    eal.save_asset(mat_path)
    L("emissive added to " + mat_path)

add_emissive("/Game/Hazard/Materials/M_Greybox", unreal.LinearColor(0.10, 0.10, 0.11, 1.0))
add_emissive("/Game/Hazard/Materials/M_Greybox_Enemy", unreal.LinearColor(0.12, 0.02, 0.02, 1.0))

L("Light fix complete.")
