# Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
[CmdletBinding()]
param([string]$Adb="",[string]$Device="",[switch]$Install,[switch]$Launch,[switch]$PullCaptures,[switch]$RestartBridge)
$ErrorActionPreference="Stop"
if ($Device -and $Device -notmatch '^[A-Za-z0-9_.:-]+$') { throw 'Invalid ADB device identifier.' }
if (!$Adb) { $Adb=Join-Path $env:LOCALAPPDATA 'Android\Sdk\platform-tools\adb.exe' }
if (!(Test-Path -LiteralPath $Adb)) { throw 'Pass -Adb with the path to the Android platform-tools adb executable.' }
$AdbArgs=@();if ($Device) { $AdbArgs=@('-s',$Device) }
function Invoke-Adb { param([string[]]$Arguments) $result=& $Adb @AdbArgs @Arguments;if ($LASTEXITCODE -ne 0){throw "ADB command failed: $($Arguments[0])"};return $result }
$Bundle=$PSScriptRoot
if (!(Test-Path -LiteralPath (Join-Path $Bundle 'cyinsightovr-bridge.jar'))) { $Bundle=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../bin/CyInsightOVR')) }
if ($Install) { Invoke-Adb @('install','-r',(Join-Path $Bundle 'CyInsightOVR-0.1.0-dev-arm64.apk')) }
if ($PullCaptures) { Invoke-Adb @('pull','/sdcard/Android/data/org.cyinsights.ovr/files/Captures',(Join-Path $PSScriptRoot 'Captures'));return }
$Identity=(Invoke-Adb @('shell','cmd','package','list','packages','-U','org.cyinsights.ovr')) -join "`n"
if ($Identity -notmatch '(?m)^package:org\.cyinsights\.ovr\s+uid:(\d+)\s*$') { throw 'Install the CyInsightOVR development APK first.' }
$OvrUid=$Matches[1]
$Bridge=Join-Path $Bundle 'cyinsightovr-bridge.jar'
if (!(Test-Path -LiteralPath $Bridge)) { throw 'Missing packaged cyinsightovr-bridge.jar.' }
# Replace the JAR inode atomically: a running app_process may still map the old DEX.
$BridgeIncoming='/data/local/tmp/cyinsightovr-bridge-incoming-'+[Guid]::NewGuid().ToString('N')+'.jar'
Invoke-Adb @('push',$Bridge,$BridgeIncoming)
Invoke-Adb @('shell','chmod','600',$BridgeIncoming)
Invoke-Adb @('shell','mv','-f',$BridgeIncoming,'/data/local/tmp/cyinsightovr-bridge.jar')
# Only this exact helper is managed. No game, profiling configuration or other ADB service is stopped.
$Existing=(& $Adb @AdbArgs shell pidof cyinsightovr-bridge) -join ' '
if ($LASTEXITCODE -gt 1) { throw 'Cannot query the developer bridge process.' }
if ($RestartBridge -and $Existing.Trim()) {
    foreach ($OvrPid in ($Existing.Trim() -split '\s+')) {
        if ($OvrPid -notmatch '^\d+$') { throw 'Invalid collector PID.' }
        $CommandLine=(Invoke-Adb @('shell','cat',"/proc/$OvrPid/cmdline")) -join ''
        if ($CommandLine.Split([char]0)[0] -ne 'cyinsightovr-bridge') { throw 'Collector process identity changed; refusing to stop it.' }
        # Older helpers did not reap their profiler on shutdown. Record only
        # verified direct children, including their start time to prevent PID reuse.
        $OwnedProfilers=@()
        foreach ($ProcessLine in (Invoke-Adb @('shell','ps','-A','-o','PID,PPID,NAME'))) {
            if ($ProcessLine -match "^\s*(\d+)\s+$OvrPid\s+ovrgpuprofiler\s*$") {
                $ChildPid=$Matches[1]
                $ChildCommand=(Invoke-Adb @('shell','cat',"/proc/$ChildPid/cmdline")) -join ''
                if ($ChildCommand.Split([char]0)[0] -notin @('/system_ext/bin/ovrgpuprofiler','/system/bin/ovrgpuprofiler','/vendor/bin/ovrgpuprofiler')) { continue }
                $ChildStat=(Invoke-Adb @('shell','cat',"/proc/$ChildPid/stat")) -join ''
                $StartTicks=(($ChildStat -replace '^\d+ \([^)]*\) ','') -split '\s+')[19]
                $OwnedProfilers+=@{Pid=$ChildPid;Start=$StartTicks}
            }
        }
        Invoke-Adb @('shell','kill',$OvrPid)
        Start-Sleep -Milliseconds 700
        foreach ($Child in $OwnedProfilers) {
            $Remaining=(& $Adb @AdbArgs shell cat "/proc/$($Child.Pid)/stat" 2>$null) -join ''
            if ($LASTEXITCODE -eq 0 -and $Remaining) {
                $CurrentStart=(($Remaining -replace '^\d+ \([^)]*\) ','') -split '\s+')[19]
                if ($CurrentStart -eq $Child.Start) { Invoke-Adb @('shell','kill','-9',$Child.Pid) }
            }
        }
    }
    $Existing=''
    Start-Sleep -Milliseconds 300
}
if ($Existing.Trim()) { Write-Host 'The developer bridge is already running. Restart the bridge after updating the APK or its application UID.' }
else {
    $Script="umask 077; CLASSPATH=/data/local/tmp/cyinsightovr-bridge.jar nohup app_process /system/bin --nice-name=cyinsightovr-bridge org.cyinsights.ovr.ShellBridge $OvrUid > /data/local/tmp/cyinsightovr-bridge.log 2>&1 < /dev/null &"
    Invoke-Adb @('shell',$Script)
    Start-Sleep -Milliseconds 500
    Invoke-Adb @('shell','cat','/data/local/tmp/cyinsightovr-bridge.log')
}
# Transfer the per-start capability privately. Never print it or put it in a
# command line. The shell file is mode 0600; run-as writes only this app's files.
$Connection=(Invoke-Adb @('shell','cat','/data/local/tmp/cyinsightovr-connection.json')) -join ''
$Config=$Connection | ConvertFrom-Json
if ($Config.token -notmatch '^[0-9a-f]{64}$' -or $Config.port -lt 1024 -or $Config.port -gt 65535) { throw 'Invalid bridge connection configuration. Use -RestartBridge after updating.' }
$Writer=New-Object System.Diagnostics.ProcessStartInfo
$Writer.FileName=$Adb
$Writer.Arguments=($AdbArgs -join ' ') + ' shell "run-as org.cyinsights.ovr sh -c ''umask 077; mkdir -p files; cat > files/developer-bridge.json''"'
$Writer.UseShellExecute=$false;$Writer.CreateNoWindow=$true;$Writer.RedirectStandardInput=$true;$Writer.RedirectStandardError=$true
$Transfer=[Diagnostics.Process]::Start($Writer)
$Transfer.StandardInput.WriteLine($Connection);$Transfer.StandardInput.Close()
$Failure=$Transfer.StandardError.ReadToEnd();$Transfer.WaitForExit()
if ($Transfer.ExitCode -ne 0) { throw "Cannot configure the app's private collector connection: $Failure" }
$Connection=$null;$Config=$null
if ($Launch) { Invoke-Adb @('shell','am','start','-a','android.intent.action.MAIN','-c','android.intent.category.LAUNCHER','-n','org.cyinsights.ovr/.MainActivity') }
Write-Host 'CyInsightOVR collector runs on the headset until reboot. GPU profiling and target restarts remain explicit in the app.'
