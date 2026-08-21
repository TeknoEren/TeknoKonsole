[CmdletBinding()]
param(
    [string]$BuildDir = "build-windows",
    [string]$StageDir = "windows\stage",
    [string]$CMakeGenerator = "Ninja",
    [string]$CMakePrefixPath = $env:CMAKE_PREFIX_PATH,
    [string]$CMakeToolchainFile = $env:CMAKE_TOOLCHAIN_FILE,
    [string]$SDL2Dll = $env:SDL2_DLL,
    [string]$InnoCompiler = $env:ISCC
)

$ErrorActionPreference = "Stop"
$repo = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$build = Join-Path $repo $BuildDir
$stage = Join-Path $repo $StageDir

function Require-Command([string]$Name) {
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "Required command '$Name' was not found in PATH."
    }
}

Require-Command "cmake"
Require-Command "windeployqt"

if (-not $InnoCompiler) {
    $isccCommand = Get-Command "ISCC.exe" -ErrorAction SilentlyContinue
    if ($isccCommand) { $InnoCompiler = $isccCommand.Source }
}
if (-not $InnoCompiler -or -not (Test-Path $InnoCompiler)) {
    throw "Inno Setup compiler not found. Set ISCC to the full path of ISCC.exe."
}

if (Test-Path $build) { Remove-Item -Recurse -Force $build }
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force -Path $stage | Out-Null

$cmakeArgs = @(
    "-S", $repo,
    "-B", $build,
    "-G", $CMakeGenerator,
    "-DCMAKE_BUILD_TYPE=Release",
    "-DCMAKE_INSTALL_PREFIX=$stage"
)
if ($CMakePrefixPath) { $cmakeArgs += "-DCMAKE_PREFIX_PATH=$CMakePrefixPath" }
if (-not $CMakeToolchainFile -and $env:VCPKG_ROOT) {
    $candidateToolchain = Join-Path $env:VCPKG_ROOT "scripts\buildsystems\vcpkg.cmake"
    if (Test-Path $candidateToolchain) { $CMakeToolchainFile = $candidateToolchain }
}
if ($CMakeToolchainFile) { $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$CMakeToolchainFile" }

Write-Host "Configuring TeknoKonsole..."
& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed." }

Write-Host "Building TeknoKonsole..."
& cmake --build $build --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }

Write-Host "Installing the application into the staging directory..."
& cmake --install $build --config Release
if ($LASTEXITCODE -ne 0) { throw "CMake install failed." }

$exe = Join-Path $stage "TeknoKonsole.exe"
if (-not (Test-Path $exe)) { throw "Expected executable was not installed: $exe" }

Write-Host "Deploying Qt runtime and QML modules..."
& windeployqt --release --no-translations --no-system-d3d-compiler --qmldir (Join-Path $repo "qml") $exe
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed." }

if (-not $SDL2Dll) {
    $candidates = @()
    if ($env:VCPKG_ROOT) { $candidates += (Join-Path $env:VCPKG_ROOT "installed\x64-windows\bin\SDL2.dll") }
    if ($env:SDL2_DIR) { $candidates += (Join-Path $env:SDL2_DIR "bin\SDL2.dll") }
    $SDL2Dll = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $SDL2Dll -or -not (Test-Path $SDL2Dll)) {
    throw "SDL2.dll was not found. Set SDL2_DLL to the runtime DLL path."
}
Copy-Item -Force $SDL2Dll (Join-Path $stage "SDL2.dll")

$iss = Join-Path $repo "packaging\windows\TeknoKonsole.iss"
$installerDir = Join-Path $repo "windows\installer"
New-Item -ItemType Directory -Force -Path $installerDir | Out-Null
Write-Host "Building the installer with Inno Setup..."
Push-Location $repo
try {
    & $InnoCompiler "/Qp" "/DStagingDir=$stage" "/O$installerDir" $iss
    if ($LASTEXITCODE -ne 0) { throw "Inno Setup compilation failed." }
} finally {
    Pop-Location
}

Write-Host "Installer created under $installerDir"
