Set-StrictMode -Version Latest

function Get-WireToneJavaInfo {
    [CmdletBinding()]
    param(
        [int]$MinimumMajorVersion = 17
    )

    $candidates = New-Object System.Collections.Generic.List[object]

    if (-not [string]::IsNullOrWhiteSpace($env:JAVA_HOME)) {
        $candidates.Add([pscustomobject]@{
            Label = "JAVA_HOME"
            Home  = $env:JAVA_HOME
        })
    }

    $candidates.Add([pscustomobject]@{
        Label = "Android Studio bundled JDK"
        Home  = (Join-Path $env:ProgramFiles "Android\Android Studio\jbr")
    })

    $candidates.Add([pscustomobject]@{
        Label = "Android Studio per-user bundled JDK"
        Home  = (Join-Path $env:LOCALAPPDATA "Programs\Android Studio\jbr")
    })

    $pathJava = Get-Command "java.exe" -ErrorAction SilentlyContinue
    if ($null -ne $pathJava) {
        $pathHome = Split-Path (Split-Path $pathJava.Source -Parent) -Parent
        $candidates.Add([pscustomobject]@{
            Label = "PATH"
            Home  = $pathHome
        })
    }

    $seen = @{}

    foreach ($candidate in $candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate.Home)) {
            continue
        }

        $javaHomePath = [System.IO.Path]::GetFullPath($candidate.Home)
        if ($seen.ContainsKey($javaHomePath)) {
            continue
        }

        $seen[$javaHomePath] = $true
        $javaExe = Join-Path $javaHomePath "bin\java.exe"

        if (-not (Test-Path $javaExe)) {
            continue
        }

        # java.exe writes its normal version banner to stderr.
        # Windows PowerShell 5.1 can turn that stderr stream into a
        # NativeCommandError when the caller uses ErrorActionPreference=Stop.
        # Temporarily allow the native command to complete, then inspect its
        # actual exit code and normalize every output item to plain text.
        $previousErrorActionPreference = $ErrorActionPreference

        try {
            $ErrorActionPreference = "Continue"
            $versionLines = & $javaExe -version 2>&1
            $javaExitCode = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $previousErrorActionPreference
        }

        if ($javaExitCode -ne 0) {
            continue
        }

        $versionText = (
            ($versionLines | ForEach-Object { $_.ToString() }) |
            Out-String
        ).Trim()

        $major = $null

        if ($versionText -match 'version\s+"(?<first>\d+)(?:\.(?<second>\d+))?') {
            $first = [int]$Matches.first
            if ($first -eq 1 -and -not [string]::IsNullOrWhiteSpace($Matches.second)) {
                $major = [int]$Matches.second
            } else {
                $major = $first
            }
        }

        if ($null -ne $major -and $major -ge $MinimumMajorVersion) {
            return [pscustomobject]@{
                Label       = $candidate.Label
                Home        = $javaHomePath
                JavaExe     = $javaExe
                Major       = $major
                VersionText = $versionText
            }
        }
    }

    return $null
}

function Get-WireToneVisualStudioInfo {
    [CmdletBinding()]
    param()

    $vsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"

    if (Test-Path $vsWhere) {
        $installationPath = (
            & $vsWhere `
                -latest `
                -products * `
                -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
                -property installationPath 2>$null |
            Select-Object -First 1
        )

        if (-not [string]::IsNullOrWhiteSpace($installationPath)) {
            return [pscustomobject]@{
                InstallationPath = $installationPath
                VsWhere          = $vsWhere
            }
        }
    }

    $patterns = @(
        (Join-Path $env:ProgramFiles "Microsoft Visual Studio\18\*"),
        (Join-Path $env:ProgramFiles "Microsoft Visual Studio\2022\*")
    )

    foreach ($pattern in $patterns) {
        $match = Get-ChildItem -Path $pattern -Directory -ErrorAction SilentlyContinue |
            Where-Object {
                Test-Path (Join-Path $_.FullName "VC\Tools\MSVC")
            } |
            Select-Object -First 1

        if ($null -ne $match) {
            return [pscustomobject]@{
                InstallationPath = $match.FullName
                VsWhere          = $vsWhere
            }
        }
    }

    return $null
}

function Get-WireToneMsvcCompilerPath {
    [CmdletBinding()]
    param(
        [string]$VisualStudioInstallationPath
    )

    if ([string]::IsNullOrWhiteSpace($VisualStudioInstallationPath)) {
        return $null
    }

    $toolsetsRoot = Join-Path $VisualStudioInstallationPath "VC\Tools\MSVC"
    if (-not (Test-Path $toolsetsRoot)) {
        return $null
    }

    $toolsets = Get-ChildItem -Path $toolsetsRoot -Directory -ErrorAction SilentlyContinue |
        Sort-Object -Property Name -Descending

    foreach ($toolset in $toolsets) {
        $compiler = Join-Path $toolset.FullName "bin\Hostx64\x64\cl.exe"
        if (Test-Path $compiler) {
            return $compiler
        }
    }

    return $null
}

function Get-WireToneCMakePath {
    [CmdletBinding()]
    param(
        [string]$VisualStudioInstallationPath
    )

    $pathCommand = Get-Command "cmake.exe" -ErrorAction SilentlyContinue
    if ($null -ne $pathCommand) {
        return $pathCommand.Source
    }

    if (-not [string]::IsNullOrWhiteSpace($VisualStudioInstallationPath)) {
        $bundledCMake = Join-Path `
            $VisualStudioInstallationPath `
            "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

        if (Test-Path $bundledCMake) {
            return $bundledCMake
        }
    }

    return $null
}

function Get-WireToneNinjaPath {
    [CmdletBinding()]
    param(
        [string]$VisualStudioInstallationPath
    )

    $pathCommand = Get-Command "ninja.exe" -ErrorAction SilentlyContinue
    if ($null -ne $pathCommand) {
        return $pathCommand.Source
    }

    if (-not [string]::IsNullOrWhiteSpace($VisualStudioInstallationPath)) {
        $bundledNinja = Join-Path `
            $VisualStudioInstallationPath `
            "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"

        if (Test-Path $bundledNinja) {
            return $bundledNinja
        }
    }

    return $null
}

function Get-WireToneAndroidSdkInfo {
    [CmdletBinding()]
    param()

    $candidates = @(
        $env:ANDROID_SDK_ROOT,
        $env:ANDROID_HOME,
        (Join-Path $env:LOCALAPPDATA "Android\Sdk")
    )

    $seen = @{}

    foreach ($candidate in $candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            continue
        }

        $root = [System.IO.Path]::GetFullPath($candidate)
        if ($seen.ContainsKey($root)) {
            continue
        }

        $seen[$root] = $true
        if (-not (Test-Path $root)) {
            continue
        }

        $adb = Join-Path $root "platform-tools\adb.exe"

        return [pscustomobject]@{
            Root = $root
            Adb  = if (Test-Path $adb) { $adb } else { $null }
        }
    }

    return $null
}
