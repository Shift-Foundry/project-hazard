# Hazard — Design & Architecture

Room-scale mixed-reality roguelite survivor for **Android XR** (XREAL Project Aura), built in
**Unreal Engine 5.8**, C++-first, server-authoritative, mobile-XR-lean. Studio: **Shift Foundry**.
Repo: https://github.com/Shift-Foundry/project-hazard

Originally developed against **UE 5.6** and migrated to **UE 5.8** after its public release, to track
the current engine branch and evaluate its updated rendering, profiling, and XR platform features.

> Status: **Phases 1–6 + Phase 7 part 1 complete.** P5 game shell (menu, pause/restart, settings, **PC flat-mode**, preview room). P6 perf-safe VFX + SFX + difficulty. P7 part 1: **coins economy, data-driven rarity weapons (`DT_Weapons`/`UWeaponLibrary`, Crit affix), chest case-rolls (weapon or coins, Exclusive chest-only), bosses every N waves → milestone chests, endless.** Boots into `L_MainMenu`. P7 part 2 (shop, classes, case-spinner UI) pending.

## Build & run

- Engine: UE **5.8** at `C:\Games\UE_5.8`. Toolchain: Visual Studio 2026 (C++ game workload).
- Compile the editor target (editor must be **closed**):
  ```
  "C:\Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" HazardEditor Win64 Development -Project="C:\UnrealProjects\Hazard\Hazard.uproject" -WaitMutex
  ```
- Author assets/level (headless, editor closed):
  ```
  "C:\Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\UnrealProjects\Hazard\Hazard.uproject" -ExecutePythonScript="C:\UnrealProjects\Hazard\Scripts\hazard_phase1_authoring.py" -unattended -stdout
  ```
- Play: open the project, load `Content/Hazard/Maps/L_HazardArena`, PIE (flat) or VR Preview.
- **Operational gotchas** (build / headless validation / config keys / Android-XR engine gap / perf
  pitfalls): see [`DEVNOTES.md`](DEVNOTES.md). Read it before touching anchors, input, or packaging.

## Module / plugin layout

```
Source/Hazard/                  Primary game module "Hazard" (all gameplay logic — C++)
Plugins/ShiftXRCore/            Reusable XR core plugin (hands, anchors, perf governor)
Scripts/                        Python editor-authoring scripts (assets, BPs, levels)
Content/Hazard/                 Python-authored assets (Materials, Meshes, Blueprints, Maps)
```

**Work split (mandatory):** all gameplay logic is authored as C++ source and compiled. To maintain
strict determinism, asset reproducibility, and source-control hygiene, all editor content generation
and asset benchmarking are automated via headless **Python scripting** (see `Scripts/`). Editor-side
manual authoring is explicitly restricted; Blueprints function strictly as thin data shells and carry
**zero graph logic** (mesh slots / default values on the C++ classes).

## ShiftXRCore plugin (reusable)

| Class | Base | Responsibility |
|---|---|---|
| `UShiftXRHandInteractor` | `UActorComponent` | Reads hand tracking via the `IHandTracker` modular feature (26 `EHandKeypoint` joints, world-space), derives an aim ray and pinch/grab/push/**shift** gestures, replicates a coarse `FShiftHandPose` for co-op. |
| `UShiftXRSceneAnchor` | `USceneComponent` | Backend-agnostic scene anchoring & environment raycasts. Ships `FShiftSceneBackend_Stub` on UE 5.8; real Android XR / Quest backends are gated behind `WITH_ANDROIDXR`. |
| `UShiftXRPerfGovernor` | `UTickableWorldSubsystem` | Smooths frame time, walks a hysteresis tier ladder (`EShiftPerfTier`), maps each tier to a dynamic-resolution target + concurrent-enemy budget, logs every tier change. |

### Scene backend matrix
| Backend | Build gate | Status | Notes |
|---|---|---|---|
| `FShiftSceneBackend_Stub` | always | **active** | Anchors = world transforms; surfaces via physics line-trace against the level floor. Works on stock UE 5.8. |
| `FShiftSceneBackend_AndroidXR` | `WITH_ANDROIDXR` | not built | Real planes/anchors/persistence. **Blocked**: Google's Android XR Unreal plugins target UE 5.6, not 5.8. Wire in when a 5.8 build ships. |
| `FShiftSceneBackend_OpenXRFB` | `WITH_ANDROIDXR` | not built | Meta Quest spatial-entity path (baseline compatibility). |

## Gameplay (Source/Hazard) — Phase 1

| Class | Base | Role |
|---|---|---|
| `AHazardGameMode` | `AGameModeBase` | Server-only. Owns `UWaveDirector`, feeds it wave config. DefaultPawn = `AHazardPlayerPawn`, GameState = `AHazardGameState`, default `ZombieClass = AZombieBase`. |
| `AHazardGameState` | `AGameStateBase` | Replicated wave index, alive/remaining enemy counts, base-health %. |
| `AHazardPlayerPawn` | `APawn` | HMD-locked camera + two `UShiftXRHandInteractor` hands (sphere palm markers) + `UHealthComponent`. Stage tracking origin; no artificial locomotion. |
| `UHealthComponent` | `UActorComponent` | Replicated, server-authoritative health; `OnHealthChanged` / `OnDeath`. |
| `AHazardBase` | `AActor` | Defended objective. `UShiftXRSceneAnchor` + replicated `UHealthComponent` + single swappable mesh slot (SM_GB_Base). |
| `AZombieBase` | `APawn` | Server-authoritative steering toward Base/player; replicated movement; attacks on contact; single mesh slot (SM_GB_Zombie). |
| `UWaveDirector` | `UActorComponent` | Server-only wave spawner on a ring around the Base; obeys `UShiftXRPerfGovernor::GetMaxConcurrentEnemies()`; escalates each wave. |

## Combat (Phase 2)

| Class | Base | Role |
|---|---|---|
| `UDamageSystem` | `UWorldSubsystem` | Central server-auth damage routing; scores kills, broadcasts `OnActorKilled`. |
| `AWeaponBase` | `AActor` | Two modes — ranged (hitscan / pooled projectile, ammo + reload) and melee (overlap arc). Server fire + multicast FX. Enemies only (no friendly fire). |
| `UWeaponComponent` | `UActorComponent` | Per-hand manager; triggers from gestures (VR), mouse (desktop), or `hazard.AutoFire` (debug). Aim = hand ray or camera. |
| `AHazardProjectile` + `UProjectilePoolSubsystem` | `AActor` / `UWorldSubsystem` | Reuse pool for projectile-mode fire. |
| `AHazardHUD` | `AHUD` | Canvas HUD: health, ammo, wave, enemies, score, base %. |

Enemy archetypes (data-driven in C++ via `FZombieArchetypeStats`): **Walker** (baseline), **Runner** (fast/weak), **Brute** (slow/tanky). `WaveDirector` spawns an escalating mix; one `BP_Zombie` covers all three. Hit-react (scale pop) + death (collapse) FX multicast to clients. Controls: VR pinch = ranged, swing = melee; desktop LMB = ranged, RMB/F = melee.

## Roguelite loop (Phase 3)

| Class | Base | Role |
|---|---|---|
| `URunManager` | `UWorldSubsystem` | Level → clear → card → harder-level loop; stackable run modifiers (queried live by weapons); high-score persistence. |
| `UCardSystem` | `UWorldSubsystem` | Offers 3 random cards from `DT_Cards` (C++ fallback); effects stack as run modifiers. |
| `AChestActor` | `AActor` | Placeable, timer-opens, rolls `ULootSystem` loot, applies via RunManager. |
| `ULootSystem` | `UWorldSubsystem` | Weighted loot roll from `DT_Loot` (C++ fallback). |
| `UHazardSaveGame` | `USaveGame` | Persistent high score / best level (slot `HazardRun`). |

DataTables: `DT_Cards`, `DT_Loot`, `DT_Enemies` (the last drives `FZombieArchetypeStats`). Each system falls back to C++ defaults if its table is absent. Card pick: HUD shows 3 choices, press **1/2/3**. Chest: press **C** to place (opens on a timer).

## Co-op (Phase 4)

| Class | Base | Role |
|---|---|---|
| `AHazardPlayerController` | `APlayerController` | Server RPCs for card pick / chest placement (same path single-player & co-op). |
| `UColocationComponent` | `UActorComponent` | Aligns each local player to the shared room anchor (`GameState::SharedAnchorTransform`). Stub-identity on UE 5.8; `AlignToSharedAnchor()` is the real-anchor hook. |

Server-authoritative throughout; run/card/score/colocation state replicates via `AHazardGameState`. `AHazardGameMode` handles join/leave; the run ends when the Base falls or all players die. Test co-op locally via PIE *Number of Players = 2, Play As Listen Server*. On-device 1:1 colocation awaits a UE-5.8 Android XR anchor plugin (see backend matrix).

## Networking model

Server-authoritative from day one (ships co-op via listen server in Phase 4).
- Health, wave/game state, zombie movement: replicated; damage & steering run server-side only.
- Hand pose: local read drives visuals/gameplay; a throttled coarse `FShiftHandPose` replicates
  (full 26-joint skeleton stays local). Client→server push via an unreliable Server RPC.

## Greybox art

Gray placeholders only. Materials `M_Greybox` (gray) / `M_Greybox_Enemy` (red). Meshes named
`SM_GB_*` (duplicated engine primitives for now). Each visual lives in a **single swappable mesh
slot** so real art drops in later. C++ constructors also bake an engine-primitive fallback so the
game is visible even before Blueprint assignment.

## Mobile-XR-lean rendering

Forward Shading, Mobile Multi-View, Instanced Stereo, MobileHDR off. Ray Tracing, Substrate,
Lumen (DynamicGI + reflections), Virtual Shadow Maps, Mesh Distance Fields all **off**. Budget:
≤250 draw calls, ≤350k visible tris, locked 90 fps target.

## Known limitations / deferred

- **C1 — Android XR scene/anchors not in UE 5.8.** Anchoring & co-op colocation run on the stub
  backend; real device anchoring deferred to a 5.8-compatible Google plugin.
- **C2 — Foveation/VRS/SpaceWarp are boot-time (`ECVF_ReadOnly`) in 5.8.** Configured via ini;
  the PerfGovernor governs at runtime via dynamic resolution + scalability + gameplay budgets.
- **Hands require a headset** (VR Preview / device); flat PIE validates waves/Base/replication.
- **Python cannot author Blueprint graph logic** — all logic stays in C++ by design.
