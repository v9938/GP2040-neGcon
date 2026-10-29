[CmdletBinding()]
param(
    [string]$PicoSdkPath = $env:PICO_SDK_PATH,
    [string]$ToolchainPath = $env:SDK_TOOLCHAIN,
    [string]$PicotoolDir = $env:picotool_DIR,
    [string]$PythonExecutable,
    [string]$CMakeExecutable = 'cmake',
    [string]$Generator,
    [ValidatePattern('^\d{8}$')][string]$ReleaseDate = (Get-Date -Format yyyyMMdd),
    [ValidateRange(1, 256)][int]$Jobs = 1,
    [switch]$RebuildWeb,
    [switch]$InstallPythonRequirements,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$build = Join-Path $root 'build-release'
$releaseVersion = '0.7.12-neGcon'
$null = [datetime]::ParseExact($ReleaseDate, 'yyyyMMdd', [Globalization.CultureInfo]::InvariantCulture)
if ((Test-Path -LiteralPath $build) -and ((Get-Item -LiteralPath $build).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
    throw 'Refusing to use a linked build-release directory.'
}

function Invoke-Checked {
    param([string]$Executable, [string[]]$Arguments)
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Executable failed (exit $LASTEXITCODE)." }
}

function Resolve-Tool {
    param([string]$Name)
    $command = Get-Command $Name -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $command) { throw "Required tool not found: $Name. Install it and add it to PATH, or supply its parameter." }
    return $command.Source
}

if ($Clean) {
    # Only this wrapper's build output is removable, never the source or a distribution directory.
    if (Test-Path -LiteralPath $build) {
        if ((Get-Item -LiteralPath $build).Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw 'Refusing to clean a linked build-release directory.'
        }
        Remove-Item -LiteralPath $build -Recurse -Force
    }
    return
}

$homeDirectory = if ($env:USERPROFILE) { $env:USERPROFILE } else { $env:HOME }
$picoHome = if ($homeDirectory) { Join-Path $homeDirectory '.pico-sdk' } else { $null }
if (-not $PicoSdkPath -and $picoHome) {
    $candidate = Join-Path $picoHome 'sdk/2.1.1'
    if (Test-Path -LiteralPath (Join-Path $candidate 'pico_sdk_init.cmake')) { $PicoSdkPath = $candidate }
}
if (-not $PicoSdkPath -or -not (Test-Path -LiteralPath (Join-Path $PicoSdkPath 'pico_sdk_init.cmake'))) {
    throw 'Pico SDK 2.1.1 is external. Set PICO_SDK_PATH or pass -PicoSdkPath; see docs/BUILD-neGcon.md.'
}
$PicoSdkPath = (Resolve-Path -LiteralPath $PicoSdkPath).Path
$sdkVersionFile = Join-Path $PicoSdkPath 'pico_sdk_version.cmake'
if (-not (Test-Path -LiteralPath $sdkVersionFile)) { throw 'Pico SDK version file is missing.' }
$sdkVersion = Get-Content -LiteralPath $sdkVersionFile -Raw
foreach ($part in @(@('MAJOR', '2'), @('MINOR', '1'), @('REVISION', '1'))) {
    if ($sdkVersion -notmatch "set\(PICO_SDK_VERSION_$($part[0])\s+$($part[1])\)") {
        throw 'This release supports exactly Pico SDK 2.1.1.'
    }
}

# Pico extension installations are optional. Prefer tools already available on PATH.
if ($picoHome) {
    foreach ($pattern in @('cmake/*/bin', 'ninja/*', 'toolchain/14_2_Rel1/bin', 'picotool/2.1.1', 'picotool/2.1.1/bin')) {
        foreach ($directory in @(Get-Item -Path (Join-Path $picoHome $pattern) -ErrorAction SilentlyContinue)) {
            if ($directory.PSIsContainer) { $env:PATH += [IO.Path]::PathSeparator + $directory.FullName }
        }
    }
}
if (-not $ToolchainPath) { $ToolchainPath = $env:PICO_TOOLCHAIN_PATH }
if ($ToolchainPath) {
    if (Test-Path -LiteralPath (Join-Path $ToolchainPath 'bin')) { $ToolchainPath = Join-Path $ToolchainPath 'bin' }
    $ToolchainPath = (Resolve-Path -LiteralPath $ToolchainPath).Path
    $env:PATH = $ToolchainPath + [IO.Path]::PathSeparator + $env:PATH
}
$cmake = Resolve-Tool $CMakeExecutable
$compiler = Resolve-Tool 'arm-none-eabi-gcc'
$compilerVersion = & $compiler --version
if ($LASTEXITCODE -ne 0 -or ($compilerVersion -join "`n") -notmatch '14[._]2[._]Rel1') {
    throw 'Install Arm GNU Toolchain 14_2_Rel1 (14.2.Rel1); set SDK_TOOLCHAIN or pass -ToolchainPath.'
}
$ToolchainPath = Split-Path -Parent $compiler
if (-not $PicotoolDir -and $picoHome) {
    $candidate = Join-Path $picoHome 'picotool/2.1.1/picotool'
    if (Test-Path -LiteralPath (Join-Path $candidate 'picotoolConfig.cmake')) { $PicotoolDir = $candidate }
}
if (-not $PicotoolDir -or -not (Test-Path -LiteralPath (Join-Path $PicotoolDir 'picotoolConfig.cmake'))) {
    throw 'Install picotool 2.1.1 locally and pass -PicotoolDir (directory containing picotoolConfig.cmake), or set picotool_DIR. No SDK picotool download is permitted.'
}
$PicotoolDir = (Resolve-Path -LiteralPath $PicotoolDir).Path

foreach ($required in @('lib/tinyusb/src/tusb.h', 'lib/pico_pio_usb/CMakeLists.txt',
        'lib/nanopb/generator/nanopb_generator.py', 'lib/ArduinoJson/CMakeLists.txt')) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $required))) {
        throw "Full source dependency is missing: $required. No automatic download is performed."
    }
}
if (-not $RebuildWeb -and -not (Test-Path -LiteralPath (Join-Path $root 'lib/httpd/fsdata.c'))) {
    throw 'Prebuilt lib/httpd/fsdata.c is missing. Restore the complete source archive or explicitly use -RebuildWeb.'
}

if (-not $PythonExecutable) { $PythonExecutable = $env:Python3_EXECUTABLE }
if (-not $PythonExecutable) { $PythonExecutable = $env:PYTHON_EXECUTABLE }
if (-not $PythonExecutable) {
    $pythonCommand = Get-Command python3, python -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($pythonCommand) { $PythonExecutable = $pythonCommand.Source }
}
if (-not $PythonExecutable) { throw 'Python 3 is required. Pass -PythonExecutable or set Python3_EXECUTABLE/PYTHON_EXECUTABLE.' }
$python = Resolve-Tool $PythonExecutable
$venvPython = if ($env:OS -eq 'Windows_NT') { Join-Path $build 'venv/Scripts/python.exe' } else { Join-Path $build 'venv/bin/python' }
if ($InstallPythonRequirements) {
    Invoke-Checked $python @('-m', 'venv', (Join-Path $build 'venv'))
    Invoke-Checked $venvPython @('-m', 'pip', '--disable-pip-version-check', 'install', '-r', (Join-Path $root 'lib/nanopb/extra/requirements.txt'))
    $python = $venvPython
} elseif (Test-Path -LiteralPath $venvPython) {
    $python = $venvPython
}
try {
    Invoke-Checked $python @('-c', 'import google.protobuf, grpc_tools, pkg_resources; from importlib.metadata import version; assert version("grpcio-tools") == "1.71.0"; assert int(version("setuptools").split(".")[0]) < 81; assert tuple(int(x) for x in version("protobuf").split(".")[:2]) >= (3, 19)')
} catch {
    throw 'Selected Python lacks compatible nanopb requirements. Prepare an offline environment or explicitly pass -InstallPythonRequirements; see docs/BUILD-neGcon.md.'
}

$configure = @('-S', $root, '-B', $build,
    '-DCMAKE_BUILD_TYPE=Release', '-DSKIP_SUBMODULES=ON', '-DSKIP_WEBBUILD=ON', '-DGP2040_USE_SYSTEM_PYTHON=ON',
    '-DFETCHCONTENT_FULLY_DISCONNECTED=ON', '-DPICO_SDK_FETCH_FROM_GIT=OFF',
    '-DPICO_BOARD=seeed_xiao_rp2040', '-DGP2040_BOARDCONFIG=SeeedXIAORP2040',
    "-DPICO_SDK_PATH=$PicoSdkPath", "-DPICO_TOOLCHAIN_PATH=$ToolchainPath",
    "-Dpicotool_DIR=$PicotoolDir", '-DPICOTOOL_FORCE_FETCH_FROM_GIT=OFF',
    "-DPython3_EXECUTABLE=$python", "-DPROJECT_RELEASE_VERSION=$releaseVersion", "-DGIT_REPO_BUILD_ID=$ReleaseDate")
$env:SKIP_WEBBUILD = 'ON'
$env:SKIP_SUBMODULES = 'ON'
$env:PICOTOOL_FORCE_FETCH_FROM_GIT = 'OFF'
if ($Generator) {
    $configure += @('-G', $Generator)
} elseif (-not (Test-Path -LiteralPath (Join-Path $build 'CMakeCache.txt'))) {
    if (Get-Command ninja -CommandType Application -ErrorAction SilentlyContinue) { $configure += @('-G', 'Ninja') }
    elseif ($env:OS -eq 'Windows_NT' -and (Get-Command mingw32-make -CommandType Application -ErrorAction SilentlyContinue)) { $configure += @('-G', 'MinGW Makefiles') }
    elseif ($env:OS -ne 'Windows_NT' -and (Get-Command make -CommandType Application -ErrorAction SilentlyContinue)) { $configure += @('-G', 'Unix Makefiles') }
    else { throw 'Install Ninja (recommended) or Make, or specify a suitable -Generator.' }
}
if ($RebuildWeb) {
    $npm = Resolve-Tool 'npm'
    Push-Location (Join-Path $root 'www')
    try {
        Invoke-Checked $npm @('ci')
        Invoke-Checked $npm @('run', 'build')
    } finally { Pop-Location }
}
Invoke-Checked $cmake $configure
Invoke-Checked $cmake @('--build', $build, '--config', 'Release', '--parallel', "$Jobs")
$uf2 = Join-Path $build 'GP2040-CE_0.7.12_SeeedXIAORP2040.uf2'
if (-not (Test-Path -LiteralPath $uf2)) { throw "Expected firmware output not found: $uf2" }
$artifact = Join-Path $build "GP2040-CE_${releaseVersion}_SeeedXIAORP2040_${ReleaseDate}.uf2"
Copy-Item -LiteralPath $uf2 -Destination $artifact
Write-Output "Release UF2: $artifact"
Write-Output "Release date: $ReleaseDate (default: local OS date; override: -ReleaseDate yyyyMMdd)"
Write-Output 'Only the UF2 is a firmware distribution artifact; ELF/debug output is not copied or flashed.'
