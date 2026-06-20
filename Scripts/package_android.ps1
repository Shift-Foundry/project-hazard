<#
  package_android.ps1 — cook + package Hazard for Android XR (XREAL Aura).

  Prerequisite (one-time): install the Android SDK/NDK via Unreal's toolchain
  (UE 5.8 -> Engine/Extras/Android/SetupAndroid) so Turnkey can find them.

  Usage:
    pwsh Scripts/package_android.ps1                 # Development build, ASTC, no install
    pwsh Scripts/package_android.ps1 -Config Shipping
    pwsh Scripts/package_android.ps1 -Deploy         # also install+launch on a connected device

  Output: Build/Android (APK + symbols).
#>
[CmdletBinding()]
param(
    [ValidateSet('Development','Shipping','Test')] [string]$Config = 'Development',
    [string]$Engine  = 'C:\Games\UE_5.8',
    [string]$Project = 'C:\UnrealProjects\Hazard\Hazard.uproject',
    [string]$Archive = 'C:\UnrealProjects\Hazard\Build\Android',
    [switch]$Deploy
)

$ErrorActionPreference = 'Stop'
$RunUAT = Join-Path $Engine 'Engine\Build\BatchFiles\RunUAT.bat'
if (-not (Test-Path $RunUAT)) { throw "RunUAT not found at $RunUAT — check -Engine path." }

Write-Host "== [1/2] Turnkey: verify Android SDK/NDK ==" -ForegroundColor Cyan
& $RunUAT Turnkey -command=VerifySdk -platform=Android -UpdateIfNeeded -project="$Project"

Write-Host "== [2/2] BuildCookRun: cook + stage + package (ASTC, $Config) ==" -ForegroundColor Cyan
$deployArgs = @()
if ($Deploy) { $deployArgs = @('-deploy','-run') }   # install + launch on the attached device

& $RunUAT BuildCookRun `
    -project="$Project" `
    -platform=Android `
    -cookflavor=ASTC `
    -clientconfig=$Config `
    -cook -stage -package -build -pak -archive `
    -archivedirectory="$Archive" `
    @deployArgs

Write-Host "Done. Artifact in $Archive" -ForegroundColor Green
Write-Host "Manual install/launch (if not using -Deploy):" -ForegroundColor DarkGray
Write-Host "  adb install -r <apk>" -ForegroundColor DarkGray
Write-Host "  adb shell am start -n com.shiftfoundry.hazard/com.epicgames.unreal.GameActivity" -ForegroundColor DarkGray
Write-Host "  adb logcat -s UE" -ForegroundColor DarkGray
