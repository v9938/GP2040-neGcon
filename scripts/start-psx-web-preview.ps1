[CmdletBinding()]
param(
    [ValidateSet('127.0.0.1', 'localhost', '::1')]
    [string]$BindHost = '127.0.0.1',
    [ValidateRange(1, 65535)]
    [int]$Port = 8080,
    [string]$Workspace = (Split-Path -Parent $PSScriptRoot),
    [string]$StateFile,
    [string]$Python = 'python',
    [switch]$Simulate
)

$ErrorActionPreference = 'Stop'
$runner = Join-Path $PSScriptRoot 'psx_web_preview.py'
if (!(Test-Path -LiteralPath $Workspace -PathType Container)) { throw 'Workspace not found' }
if (!(Test-Path -LiteralPath (Join-Path $Workspace 'www/build/index.html') -PathType Leaf)) {
    throw 'www/build/index.html missing. Build the existing Web UI first.'
}
if (!(Test-Path -LiteralPath $runner -PathType Leaf)) { throw 'Preview server not found' }
$arguments = @('-u', $runner, '--host', $BindHost, '--port', "$Port", '--workspace', $Workspace)
if ($StateFile) { $arguments += @('--state-file', $StateFile) }
if ($Simulate) { $arguments += '--simulate' }
'PC-only PSX Web preview. No RNDIS, COM, SWD or hardware access.'
'Saves/reset/reboot affect preview state only. Ctrl+C stops this foreground launcher.'
# The parent starts this foreground launcher using the tracked background_process tool.
& $Python @arguments
exit $LASTEXITCODE
