[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $RepoRoot

. (Join-Path $PSScriptRoot "toolchain-common.ps1")

$failures = New-Object System.Collections.Generic.List[string]

function Write-Found {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [Parameter(Mandatory = $true)]
        [string]$Value
    )

    Write-Host "[$Name] $Value"
}

function Write-Missing {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Message
    )

    Write-Warning $Message
    $failures.Add($Message)
}

Write-Host "WireTone environment check"
Write-Host "Repository: $RepoRoot"
Write-Host ""

$git = Get-Command "git.exe" -ErrorAction SilentlyContinue
if ($null -eq $git) {
    Write-Missing "Git was not found."
} else {
    Write-Found -Name "git" -Value $git.Source
    & $git.Source --version
}

$visualStudio = Get-WireToneVisualStudioInfo
if ($null -eq $visualStudio) {
    Write-Missing "Visual Studio or Build Tools with the Desktop development with C++ workload was not found."
} else {
    Write-Found -Name "visual-studio" -Value $visualStudio.InstallationPath
    Import-WireToneVisualStudioEnvironment -VsDevCmd $visualStudio.VsDevCmd

    $cl = Get-Command "cl.exe" -ErrorAction SilentlyContinue
    if ($null -eq $cl) {
        Write-Missing "Visual Studio was found, but the x64 MSVC compiler could not be activated."
    } else {
        Write-Found -Name "cl.exe" -Value $cl.Source
    }
}

$cmake = Get-WireToneCMakePath `
    -VisualStudioInstallationPath $(if ($null -ne $visualStudio) { $visualStudio.InstallationPath } else { $null })

if ([string]::IsNullOrWhiteSpace($cmake)) {
    Write-Missing "CMake was not found. Install the C++ CMake tools component in Visual Studio Installer."
} else {
    Write-Found -Name "cmake" -Value $cmake
    & $cmake --version | Select-Object -First 1
}

$ninja = Get-WireToneNinjaPath `
    -VisualStudioInstallationPath $(if ($null -ne $visualStudio) { $visualStudio.InstallationPath } else { $null })

if ([string]::IsNullOrWhiteSpace($ninja)) {
    Write-Host "[ninja] Optional; not found. The Visual Studio CMake generator can still be used."
} else {
    Write-Found -Name "ninja" -Value $ninja
    & $ninja --version
}

$java = Get-WireToneJavaInfo -MinimumMajorVersion 17
if ($null -eq $java) {
    Write-Missing "JDK 17 or newer was not found. Java 8 is not valid for Android Gradle Plugin 9.3."
} else {
    Write-Found -Name "java" -Value "$($java.JavaExe) ($($java.Label))"
    Write-Host (($java.VersionText -split "`r?`n") | Select-Object -First 1)
}

$androidSdk = Get-WireToneAndroidSdkInfo
if ($null -eq $androidSdk) {
    Write-Missing "Android SDK was not found. Install Android Studio and its SDK."
} else {
    Write-Found -Name "android-sdk" -Value $androidSdk.Root

    if ([string]::IsNullOrWhiteSpace($androidSdk.Adb)) {
        Write-Missing "Android SDK Platform Tools were not found under $($androidSdk.Root)."
    } else {
        Write-Found -Name "adb" -Value $androidSdk.Adb
        & $androidSdk.Adb version | Select-Object -First 2
    }
}

Write-Host ""

if ($failures.Count -gt 0) {
    Write-Host "WireTone environment check: FAIL ($($failures.Count) blocker(s))"
    foreach ($failure in $failures) {
        Write-Host " - $failure"
    }
    exit 1
}

Write-Host "WireTone environment check: PASS"
