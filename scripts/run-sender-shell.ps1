[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $RepoRoot

$multiConfigPath = Join-Path $RepoRoot "out\build\windows\apps\windows-sender\$Configuration\wiretone_sender.exe"
$singleConfigPath = Join-Path $RepoRoot "out\build\windows\apps\windows-sender\wiretone_sender.exe"

if (Test-Path $multiConfigPath) {
    & $multiConfigPath
    exit $LASTEXITCODE
}

if (Test-Path $singleConfigPath) {
    & $singleConfigPath
    exit $LASTEXITCODE
}

throw "Sender shell was not found. Run scripts\bootstrap-windows.ps1 first."
