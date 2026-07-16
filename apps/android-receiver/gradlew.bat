@echo off
setlocal EnableExtensions
set "APP_HOME=%~dp0"

if defined JAVA_HOME (
    set "JAVA_EXE=%JAVA_HOME%\bin\java.exe"
) else (
    set "JAVA_EXE=java.exe"
)

"%JAVA_EXE%" -version >NUL 2>&1
if errorlevel 1 (
    echo ERROR: Java 17 was not found. Set JAVA_HOME or use Android Studio's bundled JDK. 1>&2
    exit /b 1
)

set "WRAPPER_JAR=%APP_HOME%gradle\wrapper\gradle-wrapper.jar"
if not exist "%WRAPPER_JAR%" (
    echo ERROR: Gradle wrapper JAR is missing. 1>&2
    echo Run ..\..\scripts\bootstrap-android.ps1 first. 1>&2
    exit /b 1
)

"%JAVA_EXE%" -Dorg.gradle.appname=gradlew -jar "%WRAPPER_JAR%" %*
exit /b %ERRORLEVEL%
