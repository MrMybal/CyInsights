# Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
param(
    [Parameter(Mandatory=$true)][string]$Engine,
    [string]$Destination=(Split-Path -Parent $PSScriptRoot)
)
$ErrorActionPreference='Stop'
$readerEngine=(Resolve-Path -LiteralPath $Engine).Path
$readerVersion=Get-Content -LiteralPath (Join-Path $readerEngine 'Build/Build.version') -Raw | ConvertFrom-Json
if ($readerVersion.MajorVersion -ne 5 -or $readerVersion.MinorVersion -lt 5 -or $readerVersion.MinorVersion -gt 8) {
    throw 'Use the licensed installed Engine directory for UE 5.5 to 5.8.'
}
$readerVswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $readerVswhere)) { throw 'Visual Studio C++ desktop build tools are required.' }
$readerVs=(& $readerVswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)
if (-not $readerVs) { throw 'Install the Visual Studio C++ desktop build tools.' }
$readerVcvars=Join-Path $readerVs 'VC/Auxiliary/Build/vcvars64.bat'
$readerCmake=Join-Path $readerVs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$readerNinja=Join-Path $readerVs 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
if (-not (Test-Path -LiteralPath $readerCmake) -or -not (Test-Path -LiteralPath $readerNinja)) { throw 'Install the Visual Studio CMake tools.' }
$readerSourceRoot=$PSScriptRoot
$readerDestination=[IO.Path]::GetFullPath($Destination)
$readerMinor="5.$($readerVersion.MinorVersion)"
foreach ($readerName in @('Timing','Tasks','Activities')) {
    $readerSource=Join-Path $readerSourceRoot "Native$readerName"
    $readerBuild=Join-Path $readerDestination "build-native/Native$readerName-$readerMinor"
    $readerOutput=Join-Path $readerDestination "Native$readerName/$readerMinor"
    $readerBatch=Join-Path $readerDestination "build-native/build-$readerName.cmd"
    foreach ($readerPath in @($readerEngine,$readerSource,$readerBuild,$readerOutput,$readerVcvars,$readerCmake,$readerNinja)) {
        if ($readerPath -match '[\r\n"%]') { throw 'Build paths must not contain quotes, newlines or percent characters.' }
    }
    New-Item (Split-Path -Parent $readerBatch) -ItemType Directory -Force | Out-Null
    $readerCompiler=if ($readerVersion.MinorVersion -eq 8) {' -vcvars_ver=14.44'} else {''}
    $readerCommands=@"
@echo off
call "$readerVcvars"$readerCompiler
if errorlevel 1 exit /b 1
set VSLANG=1033
"$readerCmake" -S "$readerSource" -B "$readerBuild" -G Ninja -DCMAKE_MAKE_PROGRAM="$readerNinja" -DCMAKE_BUILD_TYPE=Release -DCY_UE_ENGINE="$readerEngine" -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="$readerOutput"
if errorlevel 1 exit /b 1
"$readerCmake" --build "$readerBuild" --config Release -j 2
exit /b %errorlevel%
"@
    [IO.File]::WriteAllText($readerBatch,$readerCommands,[Text.UTF8Encoding]::new($false))
    Write-Output "Building Native$readerName for $readerMinor..."
    & $env:ComSpec /d /c $readerBatch
    if ($LASTEXITCODE -ne 0) { throw "Native$readerName build failed. The CSV timing bridge remains available." }
    Copy-Item -LiteralPath (Join-Path $readerSource 'LICENSE') -Destination $readerOutput -Force
}
Write-Output 'Native readers installed. Open CyInsights and select this engine UnrealInsights.exe in Session.'
