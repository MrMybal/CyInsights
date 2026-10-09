param([string]$Python='python')
$ErrorActionPreference='Stop'
$aiInstallRoot=$PSScriptRoot
$aiInterpreter=Join-Path $aiInstallRoot 'runtime/python/Scripts/python.exe'
if (-not (Test-Path -LiteralPath $aiInterpreter)) {
    & $Python -c "import sys,struct; assert sys.version_info[:2]==(3,11) and struct.calcsize('P')==8, 'The supplied offline wheels require Python 3.11 x64'"
    if ($LASTEXITCODE -ne 0) { throw 'The supplied offline runtime requires Python 3.11 x64' }
    & $Python -m venv (Join-Path $aiInstallRoot 'runtime/python')
    if ($LASTEXITCODE -ne 0) { throw 'Python 3.11 x64 with venv is required' }
}
& $aiInterpreter -m pip install --keyring-provider disabled --no-index --find-links (Join-Path $aiInstallRoot 'wheels') -r (Join-Path $aiInstallRoot 'requirements.lock.txt')
if ($LASTEXITCODE -ne 0) { throw 'AI runtime installation failed; the native profiler remains available' }
& $aiInterpreter -m cytools_core.scaffold validate (Join-Path $aiInstallRoot 'app/CyTool.json')
if ($LASTEXITCODE -ne 0) { throw 'AI tool contract validation failed' }
Write-Output 'CyInsights AI companion ready. Open AI Inspector > Connection in the native application.'
