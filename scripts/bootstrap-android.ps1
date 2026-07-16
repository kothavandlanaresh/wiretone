[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $RepoRoot

. (Join-Path $PSScriptRoot "toolchain-common.ps1")

$git = Get-Command "git.exe" -ErrorAction SilentlyContinue
if ($null -eq $git) {
    throw "Git is required but was not found."
}

$java = Get-WireToneJavaInfo -MinimumMajorVersion 17
if ($null -eq $java) {
    throw "JDK 17 or newer was not found. Install Android Studio or set JAVA_HOME to a JDK 17+ installation."
}

$env:JAVA_HOME = $java.Home
$env:Path = "$($java.Home)\bin;$($env:Path)"
Write-Host "Using Java $($java.Major): $($java.JavaExe) ($($java.Label))"

$androidSdk = Get-WireToneAndroidSdkInfo
if ($null -eq $androidSdk) {
    throw "Android SDK was not found. Install Android Studio and complete its SDK setup."
}

$env:ANDROID_SDK_ROOT = $androidSdk.Root
$env:ANDROID_HOME = $androidSdk.Root
Write-Host "Using Android SDK: $($androidSdk.Root)"

if ([string]::IsNullOrWhiteSpace($androidSdk.Adb)) {
    throw "Android SDK Platform Tools are missing. Install Android SDK Platform-Tools in Android Studio's SDK Manager."
}

$AndroidRoot = Join-Path $RepoRoot "apps\android-receiver"
$WrapperDirectory = Join-Path $AndroidRoot "gradle\wrapper"
$WrapperJar = Join-Path $WrapperDirectory "gradle-wrapper.jar"
$WrapperUrl = "https://github.com/gradle/gradle/raw/refs/tags/v9.5.0/gradle/wrapper/gradle-wrapper.jar"
$ExpectedGitBlobSha = "b1b8ef56b44f16b14dc800fa8103a6d89abb526f"

New-Item -ItemType Directory -Path $WrapperDirectory -Force | Out-Null

if (-not (Test-Path $WrapperJar)) {
    Write-Host "Downloading the official Gradle 9.5.0 wrapper JAR"
    Invoke-WebRequest -Uri $WrapperUrl -OutFile $WrapperJar
}

$ActualGitBlobSha = (& $git.Source hash-object $WrapperJar).Trim()
if ($LASTEXITCODE -ne 0) {
    throw "Could not calculate the Gradle wrapper Git blob SHA."
}

if ($ActualGitBlobSha -ne $ExpectedGitBlobSha) {
    Remove-Item $WrapperJar -Force -ErrorAction SilentlyContinue
    throw "Gradle wrapper verification failed. Expected Git blob SHA $ExpectedGitBlobSha but received $ActualGitBlobSha."
}

Set-Location $AndroidRoot

Write-Host "Gradle wrapper verified."
.\gradlew.bat --version
if ($LASTEXITCODE -ne 0) {
    throw "Gradle wrapper startup failed."
}

Write-Host "Building the Android debug app and JNI library."
.\gradlew.bat :app:assembleDebug --stacktrace
if ($LASTEXITCODE -ne 0) {
    throw "Android debug build failed."
}

$apk = Join-Path $AndroidRoot "app\build\outputs\apk\debug\app-debug.apk"
if (-not (Test-Path $apk)) {
    throw "Gradle reported success, but the debug APK was not found at $apk."
}

Write-Host "Debug APK: $apk"
Write-Host "WireTone Android foundation: PASS"
