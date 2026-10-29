param([ValidateSet('Debug','Release')][string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
if (!(Test-Path -LiteralPath $PSScriptRoot)) { throw 'Source directory missing' }
& cmake -S $PSScriptRoot -B "$PSScriptRoot\build" -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
& cmake --build "$PSScriptRoot\build" --config $Configuration --parallel 1
if ($LASTEXITCODE -ne 0) { throw 'MSVC build failed' }
& ctest --test-dir "$PSScriptRoot\build" -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Native tests failed' }
"Executable: $PSScriptRoot\build\$Configuration\psx_profile_editor.exe"
"Preview DLL: $PSScriptRoot\build\$Configuration\analog_preview.dll"
