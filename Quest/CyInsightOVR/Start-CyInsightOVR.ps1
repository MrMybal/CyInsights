# Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
[CmdletBinding()]
param([string]$Adb="",[string]$Device="",[switch]$Install,[switch]$Launch,[switch]$PullCaptures,[switch]$RestartBridge,[switch]$HudOnly,[ValidateSet('standard','vulkan')][string]$Variant='standard')
$ErrorActionPreference="Stop"
if ($Device -and $Device -notmatch '^[A-Za-z0-9_.:-]+$') { throw 'Invalid ADB device identifier.' }
if (!$Adb) { $Adb=Join-Path $env:LOCALAPPDATA 'Android\Sdk\platform-tools\adb.exe' }
if (!(Test-Path -LiteralPath $Adb)) { throw 'Pass -Adb with the path to the Android platform-tools adb executable.' }
$AdbArgs=@();if ($Device) { $AdbArgs=@('-s',$Device) }
function Invoke-Adb { param([string[]]$Arguments) $result=& $Adb @AdbArgs @Arguments;if ($LASTEXITCODE -ne 0){throw "ADB command failed: $($Arguments[0])"};return $result }
$Bundle=$PSScriptRoot
if (!(Test-Path -LiteralPath (Join-Path $Bundle 'cyinsightovr-build.json'))) { $Bundle=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../bin/CyInsightOVR')) }
if ($Install) {
    $BuildInfoPath=Join-Path $Bundle 'cyinsightovr-build.json'
    if (Test-Path -LiteralPath $BuildInfoPath) {
        $BuildInfo=Get-Content -LiteralPath $BuildInfoPath -Raw | ConvertFrom-Json
        if ($BuildInfo.version -notmatch '^\d+\.\d+\.\d+(?:-[A-Za-z0-9.-]+)?$') { throw 'Invalid bundle version.' }
        $Flavor=if($Variant -eq 'vulkan'){'Vulkan'}else{'Standard'}
        $ExpectedName='CyInsights-v'+$BuildInfo.version+'-Quest-'+$Flavor+'-arm64.apk'
        $Entry=$BuildInfo.apks.$Variant
        if (!$Entry -or $Entry.file -ne $ExpectedName -or $Entry.sha256 -notmatch '^[a-f0-9]{64}$') { throw 'The selected APK variant is not included in this bundle.' }
        $ApkPath=Join-Path $Bundle $ExpectedName
        if ((Get-FileHash -LiteralPath $ApkPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $Entry.sha256) { throw 'APK checksum does not match the bundle manifest.' }
    } else {
        $LegacyName=if($Variant -eq 'vulkan'){'CyInsightOVR-0.1.0-dev-vulkan-arm64.apk'}else{'CyInsightOVR-0.1.0-dev-arm64.apk'}
        $ApkPath=Join-Path $Bundle $LegacyName
    }
    Invoke-Adb @('install','-r',$ApkPath)
}
if ($PullCaptures) { Invoke-Adb @('pull','/sdcard/Android/data/org.cyinsights.ovr/files/Captures',(Join-Path $PSScriptRoot 'Captures'));return }
$Identity=(Invoke-Adb @('shell','cmd','package','list','packages','-U','org.cyinsights.ovr')) -join "`n"
if ($Identity -notmatch '(?m)^package:org\.cyinsights\.ovr\s+uid:(\d+)\s*$') { throw 'Install the CyInsightOVR development APK first.' }
$OvrUid=$Matches[1]
$Permissions=(Invoke-Adb @('shell','dumpsys','package','org.cyinsights.ovr')) -join "`n"
if ($Permissions -match 'android\.permission\.WRITE_SECURE_SETTINGS') {
    # Development authorization persists across reboots. This APK uses it only
    # for the installed Meta HUD and the selected Android external GPU layer.
    Invoke-Adb @('shell','pm','grant','org.cyinsights.ovr','android.permission.WRITE_SECURE_SETTINGS')
} elseif ($HudOnly) { throw 'Update the CyInsightOVR APK first; this version does not request standalone HUD authorization.' }
if ($HudOnly) {
    if ($Launch) { Invoke-Adb @('shell','am','start','-a','android.intent.action.MAIN','-c','android.intent.category.LAUNCHER','-n','org.cyinsights.ovr/.MainActivity','--ez','open_settings','true') }
    Write-Host 'Standalone HUD authorized. Start/Stop HUD now runs directly on the Quest, including after reboot. No shell helper was started.'
    return
}
# The APK now owns and starts its GPU foreground service. This installer never
# starts a shell helper or transfers a connection capability.
if ($RestartBridge) { Write-Warning 'The GPU service is now managed in CyInsightOVR > Connection. No shell helper is started.' }
if ($Launch) { Invoke-Adb @('shell','am','start','-a','android.intent.action.MAIN','-c','android.intent.category.LAUNCHER','-n','org.cyinsights.ovr/.MainActivity') }
Write-Host 'Open CyInsightOVR on the Quest to start its GPU service. After headset reboots, open the app again; no PC bootstrap is required.'
