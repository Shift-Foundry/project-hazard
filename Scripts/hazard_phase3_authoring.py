# Hazard - Phase 3 editor authoring (headless). DataTables (cards/loot/enemies), chest mesh+BP,
# and GameMode chest wiring. Systems fall back to C++ defaults if a table is missing, so this
# is additive. Idempotent, logged.

import unreal
import json

def L(m): unreal.log("[HAZARD3] " + m)
def W(m): unreal.log_warning("[HAZARD3] " + m)

at = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary

for d in ["/Game/Hazard/Data", "/Game/Hazard/Meshes", "/Game/Hazard/Blueprints"]:
    if not eal.does_directory_exist(d):
        eal.make_directory(d)

def get_struct(py_name, script_path):
    try:
        return getattr(unreal, py_name).static_struct()
    except Exception:
        return unreal.load_object(None, script_path)

def make_dt(name, struct, rows):
    full = "/Game/Hazard/Data/" + name
    if eal.does_asset_exist(full):
        dt = unreal.load_asset(full)
    else:
        f = unreal.DataTableFactory()
        f.set_editor_property("struct", struct)
        dt = at.create_asset(name, "/Game/Hazard/Data", unreal.DataTable, f)
    if not dt:
        W("failed to create " + name)
        return None
    try:
        problems = unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(dt, json.dumps(rows))
        if problems:
            W("%s import problems: %s" % (name, str(problems)))
    except Exception as ex:
        W("fill %s failed: %s" % (name, ex))
    eal.save_asset(full)
    L("DataTable %s (%d rows)" % (name, len(rows)))
    return dt

card_struct  = get_struct("CardRow", "/Script/Hazard.CardRow")
loot_struct  = get_struct("LootRow", "/Script/Hazard.LootRow")
enemy_struct = get_struct("ZombieArchetypeStats", "/Script/Hazard.ZombieArchetypeStats")

make_dt("DT_Cards", card_struct, [
    {"Name": "Sharpshooter", "DisplayName": "Sharpshooter", "Description": "+15% weapon damage",  "Effect": "WeaponDamageUp",    "Magnitude": 0.15},
    {"Name": "RapidFire",    "DisplayName": "Rapid Fire",   "Description": "+20% fire rate",      "Effect": "FireRateUp",        "Magnitude": 0.20},
    {"Name": "HeavyBlade",   "DisplayName": "Heavy Blade",  "Description": "+30% melee damage",   "Effect": "MeleeDamageUp",     "Magnitude": 0.30},
    {"Name": "Vitality",     "DisplayName": "Vitality",     "Description": "+25 max health",      "Effect": "MaxHealthUp",       "Magnitude": 25.0},
    {"Name": "ExtendedMag",  "DisplayName": "Extended Mag", "Description": "+6 ammo capacity",    "Effect": "AmmoCapacityUp",    "Magnitude": 6.0},
    {"Name": "Velocity",     "DisplayName": "Velocity",     "Description": "+25% projectile speed","Effect": "ProjectileSpeedUp","Magnitude": 0.25},
])

make_dt("DT_Loot", loot_struct, [
    {"Name": "ScoreSmall", "Type": "Score",     "Amount": 100.0, "Weight": 3.0},
    {"Name": "HealMed",    "Type": "Heal",      "Amount": 40.0,  "Weight": 2.0},
    {"Name": "AmmoCache",  "Type": "Ammo",      "Amount": 0.0,   "Weight": 2.0},
    {"Name": "BonusBig",   "Type": "BonusCard", "Amount": 150.0, "Weight": 1.0},
])

make_dt("DT_Enemies", enemy_struct, [
    {"Name": "Walker", "MaxHealth": 30.0,  "MoveSpeed": 120.0, "AttackDamage": 10.0, "MeshScaleMul": 1.0, "ScoreValue": 10},
    {"Name": "Runner", "MaxHealth": 18.0,  "MoveSpeed": 280.0, "AttackDamage": 6.0,  "MeshScaleMul": 0.8, "ScoreValue": 15},
    {"Name": "Brute",  "MaxHealth": 110.0, "MoveSpeed": 70.0,  "AttackDamage": 28.0, "MeshScaleMul": 1.5, "ScoreValue": 30},
])

# Chest greybox mesh + BP
sm_chest_path = "/Game/Hazard/Meshes/SM_GB_Chest"
sm_chest = unreal.load_asset(sm_chest_path) if eal.does_asset_exist(sm_chest_path) else eal.duplicate_asset("/Engine/BasicShapes/Cube", sm_chest_path)
m_grey = unreal.load_asset("/Game/Hazard/Materials/M_Greybox")

def make_bp(name, parent):
    full = "/Game/Hazard/Blueprints/" + name
    if eal.does_asset_exist(full):
        return unreal.load_asset(full)
    f = unreal.BlueprintFactory()
    f.set_editor_property("parent_class", parent)
    bp = at.create_asset(name, "/Game/Hazard/Blueprints", unreal.Blueprint, f)
    eal.save_asset(full)
    L("Blueprint " + name)
    return bp

def cdo_of(bp):
    try:
        return unreal.get_default_object(bp.generated_class())
    except Exception:
        try:
            return bp.generated_class().get_default_object()
        except Exception as ex:
            W("cdo fail: %s" % ex)
            return None

bp_chest = make_bp("BP_Chest", unreal.ChestActor)
ccdo = cdo_of(bp_chest)
if ccdo:
    try:
        mc = ccdo.get_editor_property("mesh_comp")
        if mc:
            if sm_chest:
                mc.set_editor_property("static_mesh", sm_chest)
            if m_grey:
                mc.set_material(0, m_grey)
    except Exception as ex:
        W("chest mesh fail: %s" % ex)
    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp_chest)
    except Exception:
        pass
    eal.save_asset(bp_chest.get_path_name())
    L("Configured BP_Chest")

# Wire GameMode chest class
bp_gm = unreal.load_asset("/Game/Hazard/Blueprints/BP_HazardGameMode")
if bp_gm:
    gcdo = cdo_of(bp_gm)
    if gcdo and bp_chest:
        try:
            gcdo.set_editor_property("chest_class", bp_chest.generated_class())
            L("GameMode chest_class -> BP_Chest")
        except Exception as ex:
            W("gm chest wire fail: %s" % ex)
        try:
            unreal.BlueprintEditorLibrary.compile_blueprint(bp_gm)
        except Exception:
            pass
        eal.save_asset(bp_gm.get_path_name())

eal.save_directory("/Game/Hazard", False, True)
L("Phase 3 authoring complete.")
