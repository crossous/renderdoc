param(
  [Parameter(Mandatory=$true)][string]$Output,
  [string]$CaptureDll,
  [string]$ReplayRuntime,
  [string]$D3D12Sdk,
  [string]$Python = 'python',
  [switch]$GpuUpload,
  [switch]$CompatibilityWrites,
  [switch]$RetainUploadMappings
)
$ErrorActionPreference = 'Stop'
$regRoot = Split-Path -Parent $PSScriptRoot
$regOutput = [IO.Path]::GetFullPath($Output)
if(Test-Path -LiteralPath $regOutput) { throw "Output already exists: $regOutput" }
New-Item -ItemType Directory -Path $regOutput | Out-Null
if(!$CaptureDll) { $CaptureDll = Join-Path $regRoot 'x64\Release\renderdoc.dll' }
if(!$ReplayRuntime) { $ReplayRuntime = Join-Path $regRoot 'x64\Release' }
$regExecutable = Join-Path $regRoot 'analysis\d3d12-upload-general\d3d12_upload_regression.exe'
if(!$D3D12Sdk) { $D3D12Sdk = Join-Path (Split-Path -Parent $regExecutable) 'sdk616' }
$regConfig = Join-Path $env:APPDATA 'renderdoc\renderdoc.conf'
$regConfigBytes = [IO.File]::ReadAllBytes($regConfig)
[IO.File]::WriteAllBytes((Join-Path $regOutput 'config-before.xml'), $regConfigBytes)
$regArguments = @('--extended')
if($GpuUpload) { $regArguments += '--gpu-upload' }
if($CompatibilityWrites) { $regArguments += '--compat-upload-writes' }
try {
  $regXml = [xml][IO.File]::ReadAllText($regConfig)
  function Set-RegressionBoolean([string]$Path, [bool]$Value) {
    $regParent = $regXml.DocumentElement
    foreach($regPart in $Path.Split('/')) {
      $regChild = $regParent.SelectSingleNode($regPart)
      if(!$regChild) {
        $regChild = $regXml.CreateElement($regPart)
        [void]$regParent.AppendChild($regChild)
      }
      $regParent = $regChild
    }
    $regParent.SetAttribute('type', 'Boolean')
    $regParent.InnerText = $Value.ToString().ToLowerInvariant()
  }
  Set-RegressionBoolean 'Debug/OpenConsole' $false
  Set-RegressionBoolean 'D3D12/Capture/RetainUploadMappings' ([bool]$RetainUploadMappings)
  $regXml.Save($regConfig)
  & $regExecutable '-' (Join-Path $regOutput 'native') @regArguments *> (Join-Path $regOutput 'native.log')
  if($LASTEXITCODE -ne 0) { throw "Native regression exited $LASTEXITCODE" }
  & $regExecutable $CaptureDll (Join-Path $regOutput 'regression') @regArguments *> (Join-Path $regOutput 'capture.log')
  if($LASTEXITCODE -ne 0) { throw "Capture regression exited $LASTEXITCODE" }
  $regCaptures = @(Get-ChildItem -LiteralPath $regOutput -Filter '*.rdc')
  if($regCaptures.Count -ne 2) { throw "Expected two captures, got $($regCaptures.Count)" }
  foreach($regCapture in $regCaptures) {
    foreach($regPreserve in @($false, $true)) {
      $regName = $regCapture.BaseName + $(if($regPreserve) { '-preserve' } else { '-default' })
      $regArgs = @('--runtime', $ReplayRuntime, (Join-Path $PSScriptRoot 'check_d3d12_upload_regression.py'),
                   $regCapture.FullName, (Join-Path $regOutput $regName), '--validate', '--sdk', $D3D12Sdk)
      if($regPreserve) { $regArgs += '--preserve-upload' }
      & $Python (Join-Path $PSScriptRoot 'doom_rt_python.py') @regArgs *> (Join-Path $regOutput "$regName.log")
      Write-Output "$regName exit $LASTEXITCODE"
      if($LASTEXITCODE -ne 0) { throw "Replay regression failed; see $regName.log" }
    }
  }
  $regResults = @(Get-ChildItem -LiteralPath $regOutput -Filter 'result.json' -Recurse | ForEach-Object {
    $regResult = Get-Content -LiteralPath $_.FullName -Raw | ConvertFrom-Json
    [pscustomobject]@{test=$_.Directory.Name; passed=$regResult.passed; debugMessages=$regResult.debug_messages.Count}
  })
  $regResults | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $regOutput 'summary.json')
  $regResults | Format-Table -AutoSize
} finally {
  # Replay can rewrite configuration comments when loading a build with changed settings.
  # Restore the original bytes after the complete capture/replay run as well.
  [IO.File]::WriteAllBytes($regConfig, $regConfigBytes)
}
if($regResults.Count -ne 4 -or @($regResults | Where-Object { !$_.passed }).Count -gt 0) { exit 1 }
