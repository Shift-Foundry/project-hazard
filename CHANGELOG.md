# Changelog

All notable changes to Hazard, per phase.

## Phase 7 (part 1) — Economy, rarity weapons, chests, bosses (endless)

### Added
- **Coins economy**: drop from kills (Walker 1 / Runner 2 / Brute 5), replicated in `AHazardGameState`, shown on HUD.
- **Data-driven weapons** (`FWeaponDef` / `DT_Weapons` + C++ fallback): rarity **Common → Exclusive** with damage multipliers and a **Crit** affix; `AWeaponBase::InitFromDef` (replicated mesh + rarity); `UWeaponLibrary` rolls weapons by rarity.
- **Chests are case-rolls**: on open, a tier-weighted rarity roll yields a **weapon** (equipped on the nearest player) **or a coin payout**; higher milestone tier = better odds; **Exclusive** weapons are chest-only.
- **Bosses** every N waves (`hazard.BossEvery`, default 10): a scaled-up boss; on death it drops a **milestone chest** (tier scales with progress). Endless loop continues.
- Main menu **Story mode** placeholder entry (content TBD).

### Validated headlessly
`[Chest] -> Rifle [Epic]` equipped; coins accrue; `BOSS WAVE -> boss defeated -> milestone chest`; no errors.

### Fixes / polish
- **Lighting:** preview room / arena were dark (skylight set to a specified-cubemap with none assigned → no ambient). Fixed with SkyAtmosphere + real-time SkyLight + a brighter sun.
- **UI:** nicer Canvas HUD (status panel, health/base bars, crosshair, dimmed centered card/game-over/pause overlays) and a cleaner main menu (centered title, panel, footer hints).
- **Crash fix:** `hazard.BossEvery` is a static CVar; building it via Live Coding left it unconstructed → null-deref in `BeginNextWave`. Resolved by a full compile (always full-build after adding new statics; don't Ctrl+Alt+F11).

### Deferred to Phase 7 part 2
- Shop (buy / upgrade weapon, reroll, buy class), classes (`DT_Classes`), the animated case-opening spinner + reroll UI, and weapon meshes/icons in `DT_Weapons`.

## Phase 6 — Feel: VFX + audio + balance (perf-safe on XR)

### Added
- **Perf-safe VFX**: pooled emissive flash FX (`UHazardFXSubsystem` + `AHazardFlashFX`) for muzzle / impact / melee / zombie-death / chest-open — unlit, no shadow, no collision, no replication, hard-capped pool (48), local-only. `UShiftXRPerfGovernor::GetVFXScale()` scales FX density per tier (1.0 → 0.0 on Emergency) so cosmetics never cost the 90 fps budget. Debug-draw kept under `ENABLE_DRAW_DEBUG`.
- **Audio**: weapon fire sound (reuses the XR-template fire cue) played on the cosmetic multicast path; `FireSound` is a per-weapon property (swappable in BP).
- **Difficulty**: `UHazardGameInstance` persists Easy / Normal / Hard (×0.75 / ×1.0 / ×1.5) across level loads; menu Settings cycles it; `URunManager` folds it into the per-level difficulty multiplier.

### Notes
- VFX are greybox emissive flashes (full Niagara art is a later pass). Only the gunfire SFX ships — other SFX/music need audio assets. `M_FlashFX` = unlit emissive material with a `Color` param.
- Perf safety is structural (pooling + caps + tier scaling); on-device `stat unit` confirmation belongs to the device-build pass.

## Phase 5 — Game shell + PC preview

### Added
- **PC flat mode** on `AHazardPlayerPawn` (auto when no HMD): WASD move + mouse-look (free-look camera at eye height), LMB/RMB/F fire+melee, 1/2/3 cards, C chest — the whole game is now playable on a monitor without a headset.
- **Main menu** (`L_MainMenu` + `AHazardMenuGameMode`/`AHazardMenuController`/`AHazardMenuHUD`, Canvas): [1] Play (PC preview room), [2] Play (VR arena), [3] Settings (graphics tier via `UGameUserSettings`), [4] Quit.
- **Pause** (Esc/P) and **Game Over → [R] Restart / [M] Main Menu** — closes the previously dead-end game-over.
- **Preview room** (`L_PreviewRoom`): enclosed greybox room (mock scanned space) for on-screen testing.
- Game boots into the main menu (`GameDefaultMap`).

### Notes
- Menus are Canvas/keyboard-driven greybox (UMG art pass later). Validated headlessly: menu loads with `HazardMenuGameMode` and starts no waves; preview room runs the full loop with the flat pawn.

## Phase 4 — Co-op (colocated) + perf/comfort

### Added
- **Server-authoritative co-op architecture** (listen-server ready): all systems built replicated from earlier phases; run state + offered cards + score + colocation data replicate via `AHazardGameState`.
- **`AHazardPlayerController`** with reliable Server RPCs for card pick and chest placement, so the same input path works in single-player and co-op.
- **Colocation:** `UColocationComponent` aligns each local player to a shared room anchor (`GameState::SharedAnchorTransform`, established by the server from the Base). Stub-identity on UE 5.8 (no scene anchors); `AlignToSharedAnchor()` is the hook for a real Android XR anchor backend.
- **Join/leave** handling (`AHazardGameMode::PostLogin`/`Logout`); late joiners receive run state via replication.
- **Player death** handling — the run ends when all players are down (`URunManager::NotifyPlayerDied`).

### Fixed (adversarial code review)
- Replicate `UHealthComponent::MaxHealth` (+OnRep) — remote clients previously computed wrong health %/HUD after MaxHealth changed (cards/archetypes/difficulty).
- MaxHealth ("Vitality") card no longer silently revives downed players (`ResetToFull` skipped for dead pawns).
- `UWeaponComponent::EndPlay` destroys its spawned weapon actor — no server actor leak on player leave/respawn.
- `ServerPlaceChest` gated to `RunState == Playing` + per-player cooldown — closes a client-driven loot-spam exploit.

### Notes / deferred
- 2-client co-op is validated via PIE multi-client (listen server); colocation maps 1:1 on-device only once a UE-5.8 Android XR anchor plugin exists.
- A host/join lobby UI is deferred (needs an OnlineSubsystem session flow); use PIE "Number of Players = 2, Play As Listen Server" to test co-op locally.
- Comfort: room-scale only, no artificial locomotion (unchanged). Perf: PerfGovernor tiers tuned (90 fps target, dynamic-res + enemy-budget levers).

## Phase 3 — Roguelite loop (levels, cards, chests + loot, score)

### Added
- **`URunManager`** (world subsystem): drives the level → clear → card → harder-level loop with difficulty escalation; holds stackable run modifiers; persists records.
- **`UCardSystem`** (DataTable `DT_Cards`, C++ fallback): offers 3 random cards; effects (damage / fire-rate / melee / max-health / ammo / projectile-speed) stack as run modifiers queried live by weapons.
- **`AChestActor`** (placeable, timer-open) + **`ULootSystem`** (DataTable `DT_Loot`, weighted): chests roll loot (score / heal / ammo / bonus) applied via the RunManager.
- **`UHazardSaveGame`**: persistent high score + best level (slot `HazardRun`).
- **Levels:** `UWaveDirector` reworked to run one level of escalating waves on command and broadcast `OnAllWavesCleared`.
- **Data-driven enemies:** `FZombieArchetypeStats` is now a `DT_Enemies` row; zombies read it (C++ fallback).
- HUD shows level, high score, the 3 card choices (press 1/2/3), and a game-over screen. GameState gained replicated `RunState` / `CurrentLevel` / `HighScore` / `OfferedCards`.

### Editor (Python)
- DataTables `DT_Cards` (6), `DT_Loot` (4), `DT_Enemies` (3); `SM_GB_Chest` + `BP_Chest`; GameMode chest wiring.

### Validated headlessly
Level 1 cleared → picked **Sharpshooter** (dmg ×1.15 applied) → Level 2 (difficulty ×1.25) → chest opened (+100 score) → game over → high score saved (`HazardRun.sav`). Base health raised to 1000 for a defensible objective.

## Phase 2 — Combat (ranged + melee, enemy variety, HUD)

### Added
- **Damage & weapons (C++, server-authoritative):**
  - `UDamageSystem` (world subsystem) — central server-auth damage routing; increments run score and broadcasts `OnActorKilled` on a kill.
  - `AWeaponBase` — two modes: ranged (hitscan or pooled projectile, with ammo + reload) and melee (overlap arc). Server fire + multicast FX.
  - `UWeaponComponent` — per-hand manager; routes triggers from hand gestures (VR), mouse (desktop), or `hazard.AutoFire` (debug). Resolves aim from hand ray or camera.
  - `AHazardProjectile` + `UProjectilePoolSubsystem` — reuse pool for projectile-mode fire.
- **Enemy variety:** `EZombieArchetype` Walker / Runner / Brute with per-archetype stats (health, speed, damage, scale, score). `WaveDirector` spawns an escalating mix. Simple Seeking/Attacking AI state.
- **Feedback:** hit-react (scale pop on damage) and death FX (collapse), multicast to clients.
- **HUD:** `AHazardHUD` (Canvas) — health, ammo (with reload), wave, enemies, score, base %.
- **Score:** replicated `Score` on `AHazardGameState`.
- **Hand gestures:** `UShiftXRHandInteractor` now detects a melee **swing** (palm velocity) in addition to pinch.
- **Editor (Python):** `SM_GB_Pistol` / `SM_GB_Blade` meshes, `BP_GB_Pistol` (hitscan) / `BP_GB_Blade` (melee) weapon BPs, wired onto the player pawn's weapon components.

### Notes
- Ranged default = hitscan (reliable); projectile pool is available via `RangedProjectile` mode.
- Muzzle/impact/melee FX use debug draws this phase; Niagara is a later polish pass.
- Pawn weapon components carry C++ fallback modes (right=ranged, left=melee), so combat works even before the weapon BPs are wired.

## Phase 1 — Foundation (XR pawn, anchored Base, zombies advance)

### Added
- **Project converted to C++** — primary game module `Hazard` (`Source/Hazard/`), `Hazard`/`HazardEditor` targets, enabled `PythonScriptPlugin` + `EditorScriptingUtilities`.
- **`ShiftXRCore` plugin** (built from scratch) with three reusable classes:
  - `UShiftXRHandInteractor` — hand tracking (`IHandTracker`), aim ray, pinch gesture, replicated coarse pose.
  - `UShiftXRSceneAnchor` — backend-agnostic anchoring; `FShiftSceneBackend_Stub` active on UE 5.8 (real Android XR backend gated behind `WITH_ANDROIDXR`).
  - `UShiftXRPerfGovernor` — `UTickableWorldSubsystem` with hysteresis tier ladder, dynamic-resolution + enemy-budget control, logs every tier change.
- **Gameplay classes** (server-authoritative, replication-aware): `AHazardGameMode`, `AHazardGameState`, `AHazardPlayerPawn`, `UHealthComponent`, `AHazardBase`, `AZombieBase`, `UWaveDirector`.
- **Python authoring** (`Scripts/hazard_phase1_authoring.py`): `M_Greybox`/`M_Greybox_Enemy` materials, `SM_GB_Base`/`SM_GB_Zombie`/`SM_GB_Hand` meshes, BP data-shells (`BP_Zombie`, `BP_HazardBase`, `BP_HazardPlayerPawn`, `BP_HazardGameMode`), and the playable level `L_HazardArena`.
- `DESIGN.md`, `CHANGELOG.md`.

### Changed
- **Render config → mobile-XR lean**: disabled Ray Tracing, Substrate, Lumen (DynamicGI + reflections), Virtual Shadow Maps, Mesh Distance Fields. Kept Forward Shading + Mobile Multi-View + Instanced Stereo.
- **Android target → Android XR**: `bPackageForMetaQuest=False`, kept OpenXR immersive + Google OpenXR loader, added XR activity-start-mode property and a foveation boot profile (`Config/Android/AndroidEngine.ini`). Quest baseline kept structurally compatible.
- Project default GameMode → `/Script/Hazard.HazardGameMode`.

### Known gaps (deferred)
- Real Android XR scene understanding / anchors / colocation (UE 5.6-only plugin) — stub backend in use.
- Runtime foveation/VRS scripting (boot-time only in 5.8) — handled via ini + dynamic resolution.
- Hand visuals require a headset; flat PIE validates the rest.

## Baseline
- Imported the stock UE 5.8 XR Template (Blueprint-only) as the project baseline.
