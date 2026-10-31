# Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: AGPL-3.0-or-later.
# Signs only explicitly selected CyInsights binaries with an existing certificate.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Directory,
    [ValidatePattern('^[A-Fa-f0-9]{40}$')][string]$CertificateThumbprint,
    [string]$TimestampUrl,
    [string]$SignTool,
    [switch]$VerifyOnly,
    [switch]$RequireSigned
)
$ErrorActionPreference = 'Stop'
$releaseRoot = (Resolve-Path -LiteralPath $Directory).Path
if (-not (Test-Path -LiteralPath $releaseRoot -PathType Container)) { throw 'Release directory required' }
$knownNames = @('CyInsightsApp.exe','CyCaptureTool.exe','CyInsightsUpdater.exe','CyNativeTiming.exe','CyNativeTasks.exe',
    'CyNativeActivities.exe','CyInsightsGpu.addon64')
$targets = @(Get-ChildItem -LiteralPath $releaseRoot -File -Recurse | Where-Object { $knownNames -contains $_.Name })
if (-not ($targets | Where-Object { $_.Name -eq 'CyInsightsApp.exe' })) { throw 'CyInsightsApp.exe is missing; no binary will be signed or restored' }
foreach ($file in $targets) {
    # Reject junctions/symlinks anywhere below the explicitly selected release.
    $entry = $file
    while ($entry.FullName -ne $releaseRoot) {
        if ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Reparse points are not allowed in release binaries' }
        if ($entry -is [IO.FileInfo]) { $entry = $entry.Directory } else { $entry = $entry.Parent }
        if (-not $entry) { throw 'Binary escapes the selected release directory' }
    }
}
if (-not $VerifyOnly) {
    if (-not $CertificateThumbprint) { throw 'An existing trusted code-signing certificate thumbprint is required. No self-signed certificate is created.' }
    $matches = @(Get-ChildItem Cert:\CurrentUser\My,Cert:\LocalMachine\My -CodeSigningCert | Where-Object {
        $_.Thumbprint -eq $CertificateThumbprint -and $_.HasPrivateKey -and $_.NotAfter -gt (Get-Date) -and $_.NotBefore -le (Get-Date)
    })
    if ($matches.Count -ne 1) { throw 'Select one unexpired code-signing certificate with its private key accessible in a Windows certificate store' }
    $chain = New-Object Security.Cryptography.X509Certificates.X509Chain
    try { if (-not $chain.Build($matches[0])) { throw 'The selected certificate chain is not trusted. Install the correct CA chain; this script never alters trust stores.' } }
    finally { $chain.Dispose() }
    $timestampUri = $null
    if (-not [Uri]::TryCreate($TimestampUrl, [UriKind]::Absolute, [ref]$timestampUri) -or
        $timestampUri.Scheme -notin @('http','https') -or $timestampUri.UserInfo) { throw 'Provide the public RFC 3161 timestamp URL specified by the certificate provider' }
    if (-not $SignTool) {
        $kitBin = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits/10/bin'
        $SignTool = Get-ChildItem -LiteralPath $kitBin -Directory | Sort-Object Name -Descending | ForEach-Object {
            $candidate = Join-Path $_.FullName 'x64/signtool.exe'; if (Test-Path -LiteralPath $candidate) { $candidate }
        } | Select-Object -First 1
    }
    if (-not $SignTool -or (Get-AuthenticodeSignature -LiteralPath $SignTool).Status -ne 'Valid') { throw 'A signed Windows SDK signtool.exe is required' }
    foreach ($file in $targets) {
        $signArguments = @('sign','/sha1',$CertificateThumbprint,'/s','My','/fd','SHA256','/tr',$TimestampUrl,'/td','SHA256','/d','CyInsights')
        if ($matches[0].PSParentPath -like '*LocalMachine*') { $signArguments += '/sm' }
        $signArguments += $file.FullName
        & $SignTool @signArguments
        if ($LASTEXITCODE -ne 0) { throw ('Signing failed: ' + $file.Name) }
    }
}
$records = @()
$allValid = $true
foreach ($file in $targets) {
    $locked = [IO.File]::Open($file.FullName,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    try {
        $signature = Get-AuthenticodeSignature -LiteralPath $file.FullName
        $valid = $signature.Status -eq 'Valid' -and $null -ne $signature.TimeStamperCertificate
        if (-not $valid) { $allValid = $false }
        $records += [ordered]@{
            path = $file.FullName.Substring($releaseRoot.Length+1).Replace('\','/')
            sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            status = $signature.Status.ToString()
            product_version = [Diagnostics.FileVersionInfo]::GetVersionInfo($file.FullName).ProductVersion
            publisher = if ($signature.SignerCertificate) { $signature.SignerCertificate.Subject } else { $null }
            timestamped = $null -ne $signature.TimeStamperCertificate
        }
    } finally { $locked.Dispose() }
}
$report = [ordered]@{ schema_version=1; signed=$allValid; antivirus_verdict='not_established_by_signature'; files=$records }
$report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $releaseRoot 'windows-signatures.json') -Encoding UTF8
if ((-not $VerifyOnly -or $RequireSigned) -and -not $allValid) { throw 'Release contains an unsigned, untrusted or untimestamped CyInsights binary' }
Write-Output ('Verified ' + $targets.Count + ' CyInsights binaries; trusted and timestamped: ' + $allValid)
