[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $RepoRoot

if ($null -eq (Get-Command "git" -ErrorAction SilentlyContinue)) {
    throw "git is required but was not found on PATH."
}

$JavaCommand = Get-Command "java" -ErrorAction SilentlyContinue
if ($null -eq $JavaCommand) {
    $BundledJdk = Join-Path $env:ProgramFiles "Android\Android Studio\jbr"
    $BundledJava = Join-Path $BundledJdk "bin\java.exe"

    if (-not (Test-Path $BundledJava)) {
        throw "Java 17 was not found. Install Android Studio or set JAVA_HOME to a JDK 17 installation."
    }

    $env:JAVA_HOME = $BundledJdk
    $env:Path = "$($BundledJdk)\bin;$($env:Path)"
    Write-Host "Using Android Studio bundled JDK: $BundledJdk"
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

$ActualGitBlobSha = (& git hash-object $WrapperJar).Trim()
if ($LASTEXITCODE -ne 0) {
    throw "Could not calculate the Gradle wrapper Git blob SHA."
}

if ($ActualGitBlobSha -ne $ExpectedGitBlobSha) {
    Remove-Item $WrapperJar -Force -ErrorAction SilentlyContinue
    throw "Gradle wrapper verification failed. Expected Git blob SHA $ExpectedGitBlobSha but received $ActualGitBlobSha."
}

Set-Location $AndroidRoot
Write-Host "Gradle wrapper verified. Checking the Android build configuration."
.\gradlew.bat --version
if ($LASTEXITCODE -ne 0) {
    throw "Gradle wrapper startup failed."
}

Write-Host "Building WireTone Android debug APK and native library"
.\gradlew.bat :app:assembleDebug
if ($LASTEXITCODE -ne 0) {
    throw "WireTone Android debug build failed."
}

Write-Host "WireTone Android bootstrap: PASS"
