<#
.SYNOPSIS
Replays an existing DOOM RT capture with a bounded host process and checks the result.
.DESCRIPTION
Does not start the game, install DLLs, patch memory, or change injection settings.
Requires an x64 Python host and this fork's built x64 replay runtime.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Capture,
    [Parameter(Mandatory = $true)][string]$Output,
    [string]$Python = 'C:\Python314\python.exe',
    [string]$Runtime = '',
    [ValidateRange(30, 600)][int]$TimeoutSeconds = 150,
    [switch]$Validate
)
$ErrorActionPreference = 'Stop'
$rtRoot = Split-Path -Parent $PSScriptRoot
$rtCapture = (Resolve-Path -LiteralPath $Capture).Path
$rtOutput = [IO.Path]::GetFullPath($Output)
if ([string]::IsNullOrEmpty($Runtime)) { $Runtime = Join-Path $rtRoot 'x64\Release' }
$rtRuntime = (Resolve-Path -LiteralPath $Runtime).Path
$rtPython = (Get-Command $Python -ErrorAction Stop).Source
$rtReportPath = Join-Path $rtOutput 'replay-report.json'
if (Test-Path -LiteralPath $rtReportPath) {
    throw 'Use a new output directory so an earlier result cannot count as this run.'
}
New-Item -ItemType Directory -Path $rtOutput -Force | Out-Null
$rtArgs = @(
    (Join-Path $PSScriptRoot 'doom_rt_python.py'), '--runtime', $rtRuntime,
    (Join-Path $PSScriptRoot 'doom_rt_replay.py'), $rtCapture, $rtOutput,
    '--verify-rt-events'
)
if ($Validate) { $rtArgs += '--validate' }
# These are file paths and fixed options. Windows paths cannot contain quotes.
$rtArgumentString = ($rtArgs | ForEach-Object { '"' + $_ + '"' }) -join ' '
$rtProcess = Start-Process -FilePath $rtPython -ArgumentList $rtArgumentString `
    -WorkingDirectory $rtRoot -WindowStyle Hidden -PassThru `
    -RedirectStandardOutput (Join-Path $rtOutput 'stdout.log') `
    -RedirectStandardError (Join-Path $rtOutput 'stderr.log')
$rtTimedOut = -not $rtProcess.WaitForExit($TimeoutSeconds * 1000)
if ($rtTimedOut) {
    $rtActiveLog = Join-Path $rtOutput 'renderdoc.log'
    if (Test-Path -LiteralPath $rtActiveLog) {
        Copy-Item -LiteralPath $rtActiveLog -Destination ($rtActiveLog + '.retained')
    }
    # Stop only the host created by this test.
    Stop-Process -Id $rtProcess.Id -Force -ErrorAction SilentlyContinue
    throw "Replay host exceeded $TimeoutSeconds seconds; see $rtOutput."
}
$rtProcess.Refresh()
if (-not (Test-Path -LiteralPath $rtReportPath)) {
    throw "Replay produced no report; see $rtOutput."
}
$rtReport = Get-Content -LiteralPath $rtReportPath -Raw | ConvertFrom-Json
if ($rtProcess.ExitCode -ne 0 -or -not $rtReport.complete -or $rtReport.rt_events_replayed -lt 1) {
    throw "Replay failed (host exit $($rtProcess.ExitCode)); see $rtReportPath."
}
[pscustomobject]@{
    Replay = $rtReport.open_replay
    RayTracingEvents = $rtReport.rt_events_replayed
    Seconds = [math]::Round($rtReport.open_replay_seconds, 2)
    Image = Join-Path $rtOutput 'replayed-present.png'
    Report = $rtReportPath
}
