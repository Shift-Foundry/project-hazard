# Hazard — Developer Notes (operational gotchas)

Non-obvious things that bite if you don't know them. Keep this current — it's the stuff that
isn't visible from the code or the commit log.

---

## Build / compile

- **Target files must use `BuildSettingsVersion.V7`** in `Hazard.Target.cs` / `HazardEditor.Target.cs`.
  With an installed (non-source) engine, `V5` fails with a shared-build-environment error
  ("modifies properties … not allowed").
- **Editor must be closed** when compiling the editor target from the CLI, and on the *first* add
  of a new C++ module. Command:
  ```
  "C:\Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" HazardEditor Win64 Development -Project="C:\UnrealProjects\Hazard\Hazard.uproject" -WaitMutex
  ```
- **Do NOT rely on Live Coding (Ctrl+Alt+F11) after adding a `static TAutoConsoleVariable` (or any
  new static).** Live Coding can leave the new static unconstructed → null-deref crash at first use
  (this bit us with `hazard.BossEvery`). After adding statics: full CLI build, then fully close and
  reopen the editor.
- A `#if SOMEMACRO` on an **undefined** macro is a hard error here (UE builds with C4668-as-error).
  Define the macro (e.g. `WITH_ANDROIDXR=0` in `ShiftXRCore.Build.cs` `PublicDefinitions`) rather
  than leaving it undefined.

## Headless validation

- **Run map-bearing commands from PowerShell, not the Bash tool / git-bash.** MSYS rewrites
  `/Game/Hazard/Maps/X` into a Windows path (`C:/Program Files/Git/...`) and the map won't load.
- When passing `-ExecCmds` with a space, quote the whole value as one argument:
  `-ExecCmds="hazard.AutoFire 1"`. If the space splits, the CVar silently stays at its default.
- Cold boot of `UnrealEditor-Cmd` to a loaded map takes **~25–40 s**. Validation windows shorter
  than ~40 s can be killed *before* `LoadMap`, producing an empty/short log. Use ≥45 s.
- Read the full log from `Saved/Logs/Hazard.log` (stdout is block-buffered; many `Display` lines
  never reach stdout). Example:
  ```
  UnrealEditor-Cmd.exe Hazard.uproject /Game/Hazard/Maps/L_HazardArena -game -unattended -nullrhi -nosound -FORCELOGFLUSH -ExecCmds="hazard.AutoFire 1"
  ```
- Debug CVars: `hazard.AutoFire` (weapon auto-fires nearest zombie), `hazard.BossEvery` (waves
  between bosses, default 10). SaveGame slot `HazardRun` → `Saved/SaveGames/HazardRun.sav`.

## Data: where the source of truth lives

- **There is no `DT_Weapons.uasset`.** `UWeaponLibrary::GetAllWeapons()` reads the DataTable *only
  if it exists*, otherwise falls back to `UWeaponLibrary::DefaultWeapons()` in C++ — which is what
  actually ships today. Edit weapon stats/rarity in `WeaponLibrary.cpp`. If you author a real
  `DT_Weapons`, it overrides the C++ defaults, so keep them in sync or delete the C++ list.
- `DT_Enemies` / `DT_Cards` / `DT_Loot` exist; the matching C++ also has built-in fallbacks.

## Input architecture

- Two input paths run side by side, intentionally:
  - **Legacy** `BindKey`/`BindAxis` — desktop flat-mode (WASD/mouse, `1/2/3` cards, `Q`/wheel cycle,
    `C` chest, `Esc/P` pause). This is the regression net and the only thing testable in flat PIE.
  - **Enhanced Input** (additive) — XR 6DoF controllers. IMC/IA assets live in
    `Content/XRFramework/Input`. Pawn binds `IA_Shoot_Right/Left`; controller binds `IA_Menu_Toggle`.
- **Hands fire via the gesture path**, not Enhanced Input: `UShiftXRHandInteractor` → pinch = fire,
  swing = melee. This is the primary input on a hands-first device.
- **Device split:** XREAL Aura = controller + hands; Samsung Galaxy XR = hands + eyes (controllers
  optional). Aim resolves controller aim-pose → hand ray → camera (in that order).
- **Not yet controller-reachable:** card pick (1/2/3), weapon cycle, chest placement — no matching
  IA assets exist. They need new `UInputAction`s + IMC entries, then bind like `IA_Shoot`. Verify on
  a controller/PIE (watch for double-fire if a key maps to both legacy and Enhanced Input).

## Android XR — the engine gap (read before touching anchors)

- **UE 5.8 has no Android XR scene-understanding / anchor / persistence API.** Google's Unreal XR
  plugin targets UE **5.6**. So real anchors, scene mesh, depth occlusion, and same-room colocation
  cannot work yet.
- `UShiftXRSceneAnchor` ships a functional **stub backend** behind the `IShiftSceneBackend` interface.
  Backend selection is gated by `#if WITH_ANDROIDXR` (currently `0`) in `ShiftXRSceneAnchor.cpp` and
  `ColocationComponent.cpp`. Closing the gap = implement the real backend and flip the gate; the
  call sites are already shaped for a fill-in.
- Two ways to close it (decided by which OpenXR extensions the runtime advertises on first boot):
  **(A)** own `IOpenXRExtensionPlugin` calling the Android XR anchor/plane extensions →
  `FShiftSceneBackend_AndroidXR`; **(B)** port Google's 5.6 plugin to 5.8. See `README.md`.
- **Co-op is networked/remote, not same-room colocated** until a shared-anchor backend exists.
  `UColocationComponent::AlignToSharedAnchor` is an identity no-op on the stub.

## Performance governor

- `UShiftXRPerfGovernor` (world subsystem) drives **`vr.PixelDensity`** per tier (the lever that
  actually resizes the stereo eye render targets) plus `r.ScreenPercentage` for flat mode. It also
  caps `GetMaxConcurrentEnemies()` (WaveDirector honors it) and `GetVFXScale()` (FX subsystem honors
  it). Tier ladder uses a 2 s dwell + hysteresis toward an 11.1 ms (90 fps) target.
- **Caveat:** the governor smooths **CPU `DeltaTime`**, which a frame-reprojecting XR compositor can
  mask. GPU-time / OpenXR missed-frame sensing is the right upgrade — do it with real device data.

## Config key reference (5.8 — easy to get wrong)

- Texture formats: `bMultiTargetFormat_ASTC/_DXT/_ETC2` (NOT `bBuildForETC2`).
- MSAA samples: `r.Mobile.XRMSAAMode` (NOT `r.MobileMSAA` / `r.MSAACount`).
- Frame synthesis needs the master switch `xr.OpenXRFrameSynthesis=True` **and**
  `r.Velocity.DirectlyRenderOpenXRMotionVectors=True` — not merely leaving `xr.PreferFBSpaceWarp`
  unset.
- Foveation / frame-synthesis / SDK keys are **boot-time (`ECVF_ReadOnly`)** — they must be baked
  into the package, not set at runtime. (`Config/Android/AndroidEngine.ini`.)
- Tracking-origin enum in 5.8 is `EHMDTrackingOrigin::LocalFloor` (no legacy `Floor`/`Eye`); there is
  no `OnTrackingOriginChanged` delegate.
- Foveation is currently the **FB path** (`xr.OpenXRFBFoveationLevel`). Confirm the target runtime
  advertises `XR_FB_foveation`; on the Google stack it may no-op and need the vendor path.

## Lifecycle / save

- `UHazardGameInstance` binds `FCoreDelegates::ApplicationWillDeactivate/HasReactivated` →
  on focus loss (system menu, headset removed, backgrounded) it pauses and calls
  `URunManager::SaveProgress()`. This is why an OS kill while backgrounded no longer loses the run.

## What still needs the physical device (do these the day the kit arrives)

1. First Android cook + install (`Scripts/package_android.ps1`; needs the Android SDK/NDK via
   `Engine/Extras/Android/SetupAndroid`). Inspect the merged `AndroidManifest.xml`.
2. Log `xrEnumerateInstanceExtensionProperties` + supported environment blend modes → pick the
   engine-gap track (A/B) and the passthrough mode.
3. Runtime permission-grant request for `HAND_TRACKING` (the manifest declaration is in the UPL).
4. Passthrough / `EnvironmentBlendMode` request (uncertain 5.8 API — verify on device).
5. Capture a PSO cache, measure `stat gpu` / missed frames, run a thermal soak, then tune the
   governor / foveation / pixel-density against real numbers.
6. Store assets: release keystore, app icon, splash, spectator-screen mode.
