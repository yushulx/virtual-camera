# build-windows.ps1 - builds the controller GUI, the DirectShow filter and
# the Windows packages (Inno Setup installer + portable ZIP).
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File windows\scripts\build-windows.ps1
#   powershell ... -Version 1.0.0 -QTDir C:\Qt\6.8.2\msvc2022_64 -Platform x64
#
# Requirements:
#   - CMake 3.21+ (https://cmake.org) in PATH
#   - Visual Studio 2022 (C++ workload; the "Visual Studio 17 2022"
#     generator is used, so no vcvars environment is needed)
#   - Qt 6.5+ for MSVC (pass -QTDir or have windeployqt in PATH; the CI
#     installs it with install-qt-action)
#   - Inno Setup 6 (optional; without it only the portable ZIP is built)
#
# Parameters:
#   -Version     version stamped into the packages      (default 1.0.0)
#   -QTDir       Qt installation to deploy from
#   -Platform    x64 (default) or ARM64
#   -SkipInstaller  build only into build\windows

param(
    [string]$Version = "1.0.0",
    [string]$QTDir = "",
    [string]$Platform = "x64",
    [switch]$SkipInstaller
)

$ErrorActionPreference = "Stop"

# Script lives at <repo>\windows\scripts\build-windows.ps1 -> repo root is
# two levels up from this script's directory.
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$BuildDir = Join-Path $RepoRoot "build\windows"
$WindowsDir = Join-Path $RepoRoot "windows"
$StageDir = Join-Path $WindowsDir "dist\stage"
$DistDir = Join-Path $WindowsDir "dist"

function Find-WinDeployQt {
    param([string]$QtDir)
    if ($QtDir) {
        $candidate = Join-Path $QtDir "bin\windeployqt.exe"
        if (Test-Path $candidate) { return $candidate }
    }
    # CI (install-qt-action) puts Qt on PATH / in QT_ROOT_DIR.
    foreach ($envVar in @("QT_ROOT_DIR", "QTDIR")) {
        $root = [Environment]::GetEnvironmentVariable($envVar)
        if ($root) {
            $candidate = Join-Path $root "bin\windeployqt.exe"
            if (Test-Path $candidate) { return $candidate }
            # install-qt-action nests the actual Qt under the given dir.
            $found = Get-ChildItem -Path $root -Filter "windeployqt.exe" `
                -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($found) { return $found.FullName }
        }
    }
    $onPath = Get-Command "windeployqt.exe" -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }
    return $null
}

function Find-ISCC {
    $onPath = Get-Command "iscc.exe" -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }
    foreach ($base in @("${env:ProgramFiles(x86)}", "$env:ProgramFiles")) {
        $candidate = Join-Path $base "Inno Setup 6\ISCC.exe"
        if (Test-Path $candidate) { return $candidate }
    }
    return $null
}

Write-Host "==> virtual-camera $Version (platform: $Platform)"

# --- 1. CMake configure + build ------------------------------------------------

# Prefer a modern CMake: the VS-bundled one when the cmake in PATH is too
# old for the "Visual Studio 17 2022" generator (needs CMake 3.21+).
$cmakeExe = "cmake.exe"
$cmakeInPath = Get-Command "cmake.exe" -ErrorAction SilentlyContinue
if (-not $cmakeInPath) { throw "cmake.exe not found in PATH" }
$cmakeVersion = (& $cmakeInPath.Source --version | Select-Object -First 1) -replace 'cmake version ', ''
$vsCmake = Get-ChildItem "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\*" -Recurse `
    -Filter "cmake.exe" -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -like "*CMake\CMake\bin\cmake.exe" } |
    Select-Object -First 1
if ($vsCmake -and ($cmakeVersion -notmatch '^\d+' -or
        [version]($cmakeVersion -replace '^(\d+\.\d+\.\d+).*', '$1') -lt [version]"3.21")) {
    $cmakeExe = $vsCmake.FullName
    Write-Host "==> PATH cmake ($cmakeVersion) is too old; using $cmakeExe"
}

$cmakeArgs = @(
    "-S", $RepoRoot,
    "-B", $BuildDir,
    "-G", "Visual Studio 17 2022",
    "-A", $Platform,
    "-DVCAM_BUILD_TESTS=OFF"
)
# Qt for find_package(): -QTDir wins, then the install-qt-action env vars.
$qtRoot = $QTDir
if (-not $qtRoot) { $qtRoot = [Environment]::GetEnvironmentVariable("QT_ROOT_DIR") }
if (-not $qtRoot) { $qtRoot = [Environment]::GetEnvironmentVariable("QTDIR") }
if ($qtRoot) {
    $cmakeArgs += @("-DCMAKE_PREFIX_PATH=$qtRoot")
    Write-Host "==> Qt root: $qtRoot"
}
Write-Host "==> cmake $($cmakeArgs -join ' ')"
& $cmakeExe @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }

Write-Host "==> cmake --build (Release)"
& $cmakeExe --build $BuildDir --config Release
if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }

# --- 2. Stage the application + Qt runtime --------------------------------------

$AppExe = Join-Path $BuildDir "app\Release\VirtualCamera.exe"
if (-not (Test-Path $AppExe)) { throw "built app not found: $AppExe" }

$AppStage = Join-Path $StageDir "app"
$DriverStage = Join-Path $StageDir "driver"
Remove-Item -Recurse -Force $AppStage, $DriverStage -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $AppStage, $DriverStage | Out-Null

Copy-Item $AppExe $AppStage

$windeployqt = Find-WinDeployQt -QtDir $QTDir
if (-not $windeployqt) {
    throw "windeployqt.exe not found - pass -QTDir <Qt installation>"
}
Write-Host "==> windeployqt ($windeployqt)"
& $windeployqt --release --no-translations --no-opengl-sw --compiler-runtime `
    (Join-Path $AppStage "VirtualCamera.exe")
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

# --- 3. Stage the DirectShow driver --------------------------------------------

$DriverDll = Join-Path $BuildDir "windows\driver\Release\VirtualCameraDriver.dll"
if (-not (Test-Path $DriverDll)) { throw "built driver not found: $DriverDll" }
Copy-Item $DriverDll $DriverStage

Copy-Item (Join-Path $WindowsDir "installer\PORTABLE-README.txt") $StageDir

# --- 4. Portable ZIP --------------------------------------------------------------

New-Item -ItemType Directory -Force -Path $DistDir | Out-Null
$zip = Join-Path $DistDir "VirtualCamera-$Version-win64-portable.zip"
if (Test-Path $zip) { Remove-Item $zip }
Write-Host "==> portable ZIP: $zip"
Compress-Archive -Path (Join-Path $StageDir "*") -DestinationPath $zip

if ($SkipInstaller) {
    Write-Host "==> done (installer skipped): $DistDir"
    Get-ChildItem $DistDir -File | Select-Object Name, Length
    return
}

# --- 5. Inno Setup installer -------------------------------------------------------

$iscc = Find-ISCC
if (-not $iscc) {
    Write-Warning "Inno Setup 6 not found - only the portable ZIP was built."
    Write-Warning "Install it from https://jrsoftware.org/isinfo.php (choco install innosetup)."
    Get-ChildItem $DistDir -File | Select-Object Name, Length
    return
}

Write-Host "==> Inno Setup ($iscc)"
& $iscc "/DVersion=$Version" "/DStageDir=$StageDir" `
    (Join-Path $WindowsDir "installer\VirtualCamera.iss")
if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed" }

Write-Host "==> done:"
Get-ChildItem $DistDir -File | Select-Object Name, Length
