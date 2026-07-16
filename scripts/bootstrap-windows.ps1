[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $RepoRoot

. (Join-Path $PSScriptRoot "toolchain-common.ps1")

$git = Get-Command "git.exe" -ErrorAction SilentlyContinue
if ($null -eq $git) {
    throw "Git is required but was not found."
}

$visualStudio = Get-WireToneVisualStudioInfo
if ($null -eq $visualStudio) {
    throw "Visual Studio or Build Tools with the Desktop development with C++ workload was not found."
}

Write-Host "Using Visual Studio: $($visualStudio.InstallationPath)"

$cl = Get-WireToneMsvcCompilerPath `
    -VisualStudioInstallationPath $visualStudio.InstallationPath
if ([string]::IsNullOrWhiteSpace($cl)) {
    throw "The x64 MSVC compiler was not found."
}

$cmake = Get-WireToneCMakePath -VisualStudioInstallationPath $visualStudio.InstallationPath
if ([string]::IsNullOrWhiteSpace($cmake)) {
    throw "CMake was not found. Add C++ CMake tools through Visual Studio Installer."
}

$ctest = Join-Path (Split-Path $cmake -Parent) "ctest.exe"
if (-not (Test-Path $ctest)) {
    $ctestCommand = Get-Command "ctest.exe" -ErrorAction SilentlyContinue
    if ($null -eq $ctestCommand) {
        throw "ctest.exe was not found beside CMake or on PATH."
    }
    $ctest = $ctestCommand.Source
}

$BuildDirectory = Join-Path $RepoRoot "out\build\windows"

Write-Host "Compiler: $cl"
Write-Host "CMake: $cmake"
Write-Host "Configuring WireTone in $BuildDirectory"

& $cmake `
    -S $RepoRoot `
    -B $BuildDirectory `
    -A x64 `
    -DWIRETONE_BUILD_TESTS=ON

if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed."
}

Write-Host "Building WireTone ($Configuration) with a clean native target rebuild"
& $cmake `
    --build $BuildDirectory `
    --config $Configuration `
    --clean-first

if ($LASTEXITCODE -ne 0) {
    throw "Native build failed."
}

Write-Host "Running native tests"
& $ctest `
    --test-dir $BuildDirectory `
    -C $Configuration `
    --output-on-failure

if ($LASTEXITCODE -ne 0) {
    throw "Native tests failed."
}

Write-Host "WireTone native foundation: PASS"
