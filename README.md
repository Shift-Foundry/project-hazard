# Hazard

**Room-scale mixed-reality roguelite for Android XR glasses.** The dead rise through your real
walls and floor; you hold the line bare-handed, defend a base anchored to a real surface, and push
deeper every run. Built in **Unreal Engine 5.8**, C++-first, server-authoritative.

![Unreal Engine 5.8](https://img.shields.io/badge/Unreal%20Engine-5.8-0E1128)
![C++](https://img.shields.io/badge/C%2B%2B-17-00599C)
![Android XR](https://img.shields.io/badge/Android%20XR-OpenXR-3DDC84)
![Status: technical preview](https://img.shields.io/badge/status-technical%20preview-D93A2B)
[![License: source-available](https://img.shields.io/badge/license-source--available-6E6E6E)](LICENSE)

> **Technical preview.** This is an honest, in-progress build. It runs today on flat-screen PC and in
> headless validation runs; it has **not** yet run on a headset. The [roadmap](#roadmap-to-on-device)
> tracks exactly what is done, what is partial, and what still needs the device — nothing here is
> claimed as device-verified unless it says so.

*Studio: [Shift Foundry](https://shift-foundry.com) — a small Japan-based studio building mixed-reality
games for Android XR. The repo was rebuilt from a UE 5.6 prototype when 5.8 shipped, so the commit
history is short by design.*

<!-- A short flat-screen gameplay capture goes here once recorded — motion shows the loop best. -->

## Contents

- [Status](#status)
- [Engineering highlights](#engineering-highlights)
- [Build](#build) · [Run](#run-flat-screen-no-headset) · [Verify](#verify-headless)
- [What's implemented](#whats-implemented)
- [Known limitations / not done](#known-limitations--not-done)
- [Architecture](#architecture)
- [Roadmap to on-device](#roadmap-to-on-device)
- [Engine gap](#engine-gap)
- [License](#license)

## Status

| | |
|---|---|
| **Runs today** | Flat-screen PC (WASD + mouse) in PIE / standalone; headless `-game -nullrhi` validation. |
| **Not run yet** | On any headset. The project has **never been cooked, packaged, or launched on a device** — no XREAL Aura / Galaxy XR hardware here. So anything device-specific (passthrough, foveation, controller/hand input bindings, on-device framerate) is **unverified**. |
| **Co-op** | Works as a listen-server in PIE multi-client. Same-room colocation does **not** work (see [Engine gap](#engine-gap)). |

When this README says a system is "validated," it means **it builds and behaves correctly in flat-PC
or headless runs** — not that it's been confirmed on a headset. The two are kept separate on purpose.

## Engineering highlights

- **Server-authoritative from the ground up** — replication, validated server RPCs, and an OnRep-driven
  HUD, so co-op is a built-in property rather than a retrofit.
- **Mobile-lean render path** tuned for wearable glasses: forward shading, MultiView, Instanced Stereo;
  Lumen / Virtual Shadow Maps / ray tracing / Substrate / distance fields all off.
- **Performance governor** (`UShiftXRPerfGovernor`): live enemy + VFX budgets and a `vr.PixelDensity`
  lever, built for the thermal and framerate envelope that hand-worn hardware demands.
- **Pooled projectiles and pooled VFX** — no per-shot allocations on the hot path.
- **Scene-backend abstraction** (`IShiftSceneBackend` behind a `#if WITH_ANDROIDXR` gate): the real
  Android XR anchor backend drops in as a fill-in, not a rewrite — see [Engine gap](#engine-gap).
- **Data-driven combat**: weapons with rarity + a crit/affix system, an in-run RNG roller, chests as
  case-rolls, and persistent meta-progression via SaveGame.
- **Reproducible headless validation**: boots a map with `-nullrhi`, auto-fires / auto-picks, and logs
  what each system actually did — a lightweight regression net.
- **Content authored headlessly** via Python editor scripting; Blueprints are data shells only, all
  gameplay logic lives in C++.

## Build

UE 5.8 + Visual Studio 2022/2026 (C++ game workload). Close the editor first, then from the repo root
(replace `<UE_5.8>` with your engine path):

```
"<UE_5.8>\Engine\Build\BatchFiles\Build.bat" HazardEditor Win64 Development -Project="Hazard.uproject" -WaitMutex
```

Targets use `BuildSettingsVersion.V7` (V5 fails against an installed engine). More build/headless
gotchas are in [DEVNOTES.md](DEVNOTES.md).

## Run (flat-screen, no headset)

Open the project and PIE `Content/Hazard/Maps/L_MainMenu`, or launch a level directly. Controls:

- **WASD** move, **mouse** look, **LMB** fire, **RMB / F** melee
- **1 / 2 / 3** pick a card (between levels), **G** take the rolled weapon, **Q / wheel** cycle slot, **C** place a chest
- **Esc / P** pause, **R** restart (on game over), **M** main menu
- Main menu: `1` PC room, `2` arena, `6` Shop, `3` settings, `4` quit

The menu and shop are keyboard-driven for now (see [Known limitations](#known-limitations--not-done)).

## Verify (headless)

Run from **PowerShell** (git-bash mangles `/Game/...` paths), then read `Saved/Logs/Hazard.log`:

```
UnrealEditor-Cmd.exe Hazard.uproject /Game/Hazard/Maps/L_HazardArena -game -unattended -nullrhi -nosound -FORCELOGFLUSH -ExecCmds="hazard.AutoFire 1"
```

Cold boot to a loaded map is ~25–40 s, so give it ≥45 s. A healthy run logs things like:

```
[Class] Recruit applied: slots=2, +HP=0, dmgx=1.00, start=Pistol
[Roller] offer: MP5 [Uncommon]
[Roller] offer: Blade [Common]
[Run] Level 1 cleared — choose 1 of 3 cards.
```

Debug CVars: `hazard.AutoFire`, `hazard.AutoPickCards`, `hazard.AutoChest`, `hazard.BossEvery`.

## What's implemented

Gameplay (works in flat-PC + headless):

- XR pawn (HMD camera + per-hand interactors) with a flat-mode fallback for PC.
- Combat: hitscan + pooled projectiles + melee; data-driven weapons with rarity + a crit/affix system.
- Enemies: Walker/Runner/Brute archetypes, server-auth seek AI, a boss every 10 waves, endless.
- Roguelite loop: levels → waves → clear → pick 1 of 3 cards → harder level; per-run coins.
- Meta-progression: persistent coin wallet + weapon/class unlocks (SaveGame), spent in a lobby Shop.
- Classes: Recruit (free), Trooper (cheap, +HP), Operator (3 slots) — applied at run start.
- In-run weapon RNG roller (top of HUD): rerolls from your unlocked pool every ~10 s, claimed with G/grip.
- Chests as case-rolls (weapon or coins; chest-only Exclusive weapon).
- Co-op: replication / RPCs / join-leave (listen server).
- Game shell: main menu, pause/restart, settings (graphics + difficulty), preview room.

XR / platform plumbing (configured, **not** device-tested):

- Mobile-lean render: forward shading, MultiView, Instanced Stereo; Lumen/VSM/RT/Substrate/DistanceFields off.
- `UShiftXRPerfGovernor`: enemy + VFX budgets (live) and a resolution lever via `vr.PixelDensity` (untested on device).
- Android XR manifest: Google OpenXR immersive (`libopenxr.google.so`, `FULL_SPACE_UNMANAGED`), Vulkan-only,
  `uses-feature android.software.xr.api.openxr`, `HAND_TRACKING` permission, via a UPL.
- Enhanced Input mapping contexts wired additively over the legacy desktop binds.
- `IShiftSceneBackend` interface + a working stub backend behind a `#if WITH_ANDROIDXR` gate.

## Known limitations / not done

- **No on-device build yet.** No cook has been run; the package script (`Scripts/package_android.ps1`)
  exists but needs the Android SDK/NDK and a device to exercise.
- **No real spatial features.** UE 5.8 has no Android XR scene/anchor API (Google's plugin is 5.6),
  so anchors, scene mesh, depth occlusion, and same-room colocation run on a stub. Co-op is
  networked, **not** same-room colocated. See [Engine gap](#engine-gap).
- **Menu/shop are keyboard-only.** XR controller/gesture menu navigation isn't wired.
- **Passthrough not requested**, foveation path not confirmed for the Google runtime, controller aim-pose
  and runtime permission grants not verified — all need the headset.
- **Card-pick / weapon-cycle / chest** are reachable by keyboard/controller but have no dedicated XR
  input actions yet.
- Weapon/class data lives in C++ (`UWeaponLibrary`/`UClassLibrary`); there are no `DT_Weapons`/`DT_Classes`
  assets yet, and no real weapon meshes/icons (greybox).

## Architecture

- `Source/Hazard/` — game module (all gameplay logic, C++).
- `Plugins/ShiftXRCore/` — reusable XR plugin: hand interactor, scene-anchor abstraction, perf governor.
- `Scripts/` — headless Python that authors editor content (materials, meshes, BPs, levels). Blueprints
  are data shells only; no graph logic.
- Details + operational gotchas: [DESIGN.md](DESIGN.md), [DEVNOTES.md](DEVNOTES.md).

## Roadmap to on-device

Status legend: ✅ implemented + builds + flat/headless-validated (NOT device-verified) · 🟡 partial ·
⬜ planned · 🔒 needs the headset to do/verify · 🧩 blocked by the engine gap.

### M0 — boot on device (mostly config)

| # | Task | Status |
|---|---|---|
| 0.1 | TargetSDK 32 → 34 | ✅ |
| 0.2 | UPL `uses-feature android.software.xr.api.openxr` | ✅ |
| 0.3 | XR permissions (HAND_TRACKING declared; runtime grant request) | 🟡 |
| 0.4 | PackageName + project identity | ✅ |
| 0.5 | Package script ✅; first actual cook/run | 🔒 |
| 0.6 | Trim TargetPlatforms + ASTC-only + gate editor plugins | ✅ |

### M1 — playable in-headset

| # | Task | Status |
|---|---|---|
| 1.1 | Enhanced Input wired (additive over legacy) | ✅ |
| 1.2 | Fire/melee/pause reachable; card-pick/cycle/chest need XR actions | 🟡 |
| 1.3 | Request passthrough / environment blend mode | 🔒 |
| 1.4 | OpenXR controller aim-pose (prefers controller when tracked) | 🟡 |
| 1.5 | Tracking-origin Stage + logging; recenter-on-resume pending | 🟡 |
| 1.6 | App-lifecycle: focus-loss pause + save-on-background | ✅ |

### M2 — framerate on the glasses (measure → tune)

| # | Task | Status |
|---|---|---|
| 2.1 | PerfGovernor drives `vr.PixelDensity` per tier | ✅ |
| 2.2 | Sense GPU frame time / OpenXR missed-frames | 🔒 |
| 2.3 | Confirm/repair foveation for the Google runtime | 🔒 |
| 2.4 | Frame synthesis (`xr.OpenXRFrameSynthesis`) + motion vectors | ✅ |
| 2.5 | Android DeviceProfile + MSAA; Aura GPU-match rule pending | 🟡 |
| 2.6 | Bundled PSO cache | 🔒 |
| 2.7 | Streaming pool + GC + sustained-perf | 🟡 / 🔒 |
| 2.8 | Thermal soak → iterate budgets | 🔒 |

### M3 — spatial / true MR (engine-gap)

| # | Task | Status |
|---|---|---|
| 3.1 | Real anchor backend (planes / persistence) | 🧩 |
| 3.2 | Scene mesh + depth occlusion | 🧩 |
| 3.3 | Same-room co-op colocation | 🧩 |

### M4 — ship

| # | Task | Status |
|---|---|---|
| 4.1 | Release keystore + AAB + store metadata | ⬜ |
| 4.2 | Icon, splash, spectator/cast view | ⬜ |
| 4.3 | Boundary / play-space safety | 🔒 |
| 4.4 | Articulated hand mesh, binaural audio, polish | ⬜ |

## Engine gap

UE 5.8 has no official Android XR scene/anchor API — Google's Unreal plugin targets 5.6. So real anchors,
scene mesh, depth occlusion, and same-room colocation have no built-in binding yet. The project is shaped
for the bridge: an `IShiftSceneBackend` interface, a working stub, and a `#if WITH_ANDROIDXR` gate, so the
real backend is a fill-in rather than a rewrite.

Two ways to close it, decided by what the device's OpenXR runtime actually advertises (logged on first
boot via `xrEnumerateInstanceExtensionProperties`):

- **Track A (preferred):** a thin `IOpenXRExtensionPlugin` that requests the Android XR anchor/plane
  extensions and feeds `FShiftSceneBackend_AndroidXR`. Owned, no third-party release dependency.
- **Track B:** port Google's UE 5.6 plugin to 5.8 and route it through the same interface. Faster if the
  sources cooperate, more fragile across engine updates.

Per feature: anchors are the easiest to bridge; scene mesh + depth occlusion need an extra render pass;
same-room colocation needs shared/cloud anchors (often vendor-specific) and may fall back to marker/QR
alignment. Until then, co-op stays networked-only.

## License

Source-available, all rights reserved — see [LICENSE](LICENSE). The code is public for evaluation and
review; it is **not** licensed for reuse or redistribution, and third-party components (Unreal Engine,
fonts, etc.) remain under their own terms. Greybox art; not yet shipped or device-tested.
© 2026 Shift Foundry.
