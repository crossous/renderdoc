<#
.SYNOPSIS
Build Windows Release and package only runtime dependencies.
.DESCRIPTION
Use -Rebuild for a clean release, or -PackageOnly after successful builds.
Android APKs are reused from existing Android builds, not rebuilt here.
Read docs/CUSTOM_RELEASE_BUILD.md before running.
#>
[CmdletBinding()]
param([switch]$Rebuild, [switch]$PackageOnly)
$ErrorActionPreference = 'Stop'
# Use framework hashing and ZIP APIs to avoid shell module auto-loading dependencies.
Add-Type -AssemblyName System.IO.Compression.FileSystem
function Get-ReleaseHash([string]$Path) {
    $releaseHashStream = [IO.File]::OpenRead($Path)
    $releaseHashAlgorithm = [Security.Cryptography.SHA256]::Create()
    try {
        return [BitConverter]::ToString($releaseHashAlgorithm.ComputeHash($releaseHashStream)).Replace('-', '')
    } finally { $releaseHashAlgorithm.Dispose(); $releaseHashStream.Dispose() }
}
$releaseRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$releaseLogs = Join-Path $releaseRoot ("build-windows-release\" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $releaseLogs -Force | Out-Null
$releaseMSBuild = 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe'
$releaseHeader = Get-Content (Join-Path $releaseRoot 'renderdoc\api\replay\version.h') -Raw
$releaseMajor = [regex]::Match($releaseHeader, '(?m)^#define RENDERDOC_VERSION_MAJOR\s+(\d+)').Groups[1].Value
$releaseMinor = [regex]::Match($releaseHeader, '(?m)^#define RENDERDOC_VERSION_MINOR\s+(\d+)').Groups[1].Value
if (-not $releaseMajor -or -not $releaseMinor) { throw 'Version could not be read.' }
$releaseVersion = "$releaseMajor.$releaseMinor"
$releaseCommit = (& git -C $releaseRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE) { throw 'Cannot resolve build commit.' }
if (& git -C $releaseRoot status --porcelain --untracked-files=no) { throw 'Commit tracked changes before building a package.' }

if (-not $PackageOnly) {
    $releaseTarget = if ($Rebuild) { 'Rebuild' } else { 'Build' }
    $releaseOldMP = $env:CL_MPCount
    $env:CL_MPCount = '2'
    try {
        foreach ($releasePlatform in @('x86', 'x64')) {
            $releaseLog = Join-Path $releaseLogs "$releasePlatform.log"
            Write-Host "Building $releasePlatform Release ($releaseTarget); log: $releaseLog"
            & $releaseMSBuild (Join-Path $releaseRoot 'renderdoc.sln') "/t:$releaseTarget" /m:2 /nologo /v:quiet /fl `
                "/flp:logfile=$releaseLog;verbosity=normal" `
                "/p:Configuration=Release;Platform=$releasePlatform;PreferredToolArchitecture=x64;WholeProgramOptimization=false;LinkTimeCodeGeneration=Default" `
                *> (Join-Path $releaseLogs "$releasePlatform-console.log")
            if ($LASTEXITCODE) {
                Get-Content -LiteralPath $releaseLog -Tail 60 | Write-Host
                throw "$releasePlatform build failed; see $releaseLog"
            }
            Write-Host "$releasePlatform Release build passed."
        }
    } finally { $env:CL_MPCount = $releaseOldMP }
}

# Preserve prior output; verify the final absolute paths before moving directories.
foreach ($releaseDirectory in @('dist', 'package')) {
    $releasePath = [IO.Path]::GetFullPath((Join-Path $releaseRoot $releaseDirectory))
    if ((Split-Path -Parent $releasePath) -ne $releaseRoot) { throw 'Output path escaped repository.' }
    if (Test-Path -LiteralPath $releasePath) {
        Move-Item -LiteralPath $releasePath -Destination (Join-Path $releaseLogs "previous-$releaseDirectory")
    }
    New-Item -ItemType Directory -Path $releasePath | Out-Null
}
function Copy-ReleaseFile([string]$Source, [string]$Destination) {
    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) { throw "Required runtime missing: $Source" }
    New-Item -ItemType Directory -Path (Split-Path -Parent $Destination) -Force | Out-Null
    Copy-Item -LiteralPath $Source -Destination $Destination
}
function Copy-ReleaseTree([string]$Source, [string]$Destination) {
    if (-not (Test-Path -LiteralPath $Source -PathType Container)) { throw "Required runtime directory missing: $Source" }
    foreach ($releaseFile in (Get-ChildItem -LiteralPath $Source -File -Recurse)) {
        if ($releaseFile.Extension -in @('.dll', '.pyd', '.py') -or $releaseFile.Name -eq 'qt.conf') {
            $releaseRelative = $releaseFile.FullName.Substring($Source.Length).TrimStart('\')
            Copy-ReleaseFile $releaseFile.FullName (Join-Path $Destination $releaseRelative)
        }
    }
}
$releaseCore = @('renderdoc.dll', 'system_load.dll', 'renderdoc.json', 'system_load.json',
    'render_soco_doccmd.exe', 'd3dcompiler_47.dll', 'dbghelp.dll', 'symsrv.dll', 'symsrv.yes')
$releaseGUI = @('q_render_soco_doc.exe', 'python36.dll', 'python36.zip', '_ctypes.pyd',
    'Qt5Core.dll', 'Qt5Gui.dll', 'Qt5Network.dll', 'Qt5Svg.dll', 'Qt5Widgets.dll')
foreach ($releaseArch in @('32', '64')) {
    $releaseSource = Join-Path $releaseRoot $(if ($releaseArch -eq '64') { 'x64\Release' } else { 'Win32\Release' })
    $releaseStage = Join-Path $releaseRoot "dist\Release$releaseArch"
    foreach ($releaseFile in ($releaseCore + $releaseGUI)) {
        Copy-ReleaseFile (Join-Path $releaseSource $releaseFile) (Join-Path $releaseStage $releaseFile)
    }
    $releaseShim = if ($releaseArch -eq '64') { 'render_soco_docshim64.dll' } else { 'render_soco_docshim32.dll' }
    Copy-ReleaseFile (Join-Path $releaseSource $releaseShim) (Join-Path $releaseStage $releaseShim)
    Copy-ReleaseTree (Join-Path $releaseSource 'qtplugins') (Join-Path $releaseStage 'qtplugins')
    # pymodules also contains PDBs and import libraries; ship the modules alone.
    foreach ($releaseModule in @('renderdoc.pyd', 'qrenderdoc.pyd')) {
        Copy-ReleaseFile (Join-Path $releaseSource "pymodules\$releaseModule") (Join-Path $releaseStage $releaseModule)
    }
    foreach ($releaseOptional in @('shiboken2', 'PySide2')) {
        if (Test-Path (Join-Path $releaseSource $releaseOptional)) {
            Copy-ReleaseTree (Join-Path $releaseSource $releaseOptional) (Join-Path $releaseStage $releaseOptional)
        }
    }
    foreach ($releaseOptional in @('shiboken2.dll', 'select.pyd', '_hashlib.pyd', '_socket.pyd', '_ssl.pyd',
        'pyexpat.pyd', '_queue.pyd', 'libffi-7.dll', 'libffi-8.dll', 'libcrypto-3.dll', 'libssl-3.dll')) {
        if (Test-Path (Join-Path $releaseSource $releaseOptional)) {
            Copy-ReleaseFile (Join-Path $releaseSource $releaseOptional) (Join-Path $releaseStage $releaseOptional)
        }
    }
    $releaseAgility = if ($releaseArch -eq '64') { 'x64' } else { 'win32' }
    Copy-ReleaseFile (Join-Path $releaseRoot "3rdparty\d3d12\$releaseAgility\D3D12Core.dll") (Join-Path $releaseStage 'D3D12\D3D12Core.dll')
    Copy-ReleaseFile (Join-Path $releaseRoot 'LICENSE.md') (Join-Path $releaseStage 'LICENSE.md')
}
$release64 = Join-Path $releaseRoot 'dist\Release64'
foreach ($releaseFile in ($releaseCore + @('render_soco_docshim32.dll', 'D3D12\D3D12Core.dll'))) {
    Copy-ReleaseFile (Join-Path $releaseRoot "dist\Release32\$releaseFile") (Join-Path $release64 "x86\$releaseFile")
}
$releaseAndroid = @()
foreach ($releaseABI in @('arm32', 'arm64')) {
    $releaseAPK = Join-Path $releaseRoot "build-android-$releaseABI\bin\org.renderdoc.renderdoccmd.$releaseABI.apk"
    Copy-ReleaseFile $releaseAPK (Join-Path $release64 "plugins\android\org.renderdoc.renderdoccmd.$releaseABI.apk")
    $releaseAndroid += @{ abi = $releaseABI; rebuilt = $false; sha256 = (Get-ReleaseHash $releaseAPK) }
}
foreach ($releaseCLIPath in @('render_soco_doccmd.exe', 'x86\render_soco_doccmd.exe')) {
    $releaseCLI = & (Join-Path $release64 $releaseCLIPath) version 2>&1 | Out-String
    if ($LASTEXITCODE -or -not $releaseCLI.Contains($releaseCommit) -or -not $releaseCLI.Contains($releaseVersion)) {
        throw "Packaged CLI $releaseCLIPath does not match source $releaseCommit : $releaseCLI"
    }
}
$releaseFiles = @(Get-ChildItem -LiteralPath $release64 -Recurse -File | ForEach-Object {
    @{ path = $_.FullName.Substring($release64.Length + 1); bytes = $_.Length; sha256 = (Get-ReleaseHash $_.FullName) }
})
@{ version = $releaseVersion; commit = $releaseCommit; windows = @('x86', 'x64');
    ltCG = $false; android = $releaseAndroid; files = $releaseFiles } |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $release64 'package-manifest.json') -Encoding UTF8
$releasePackage = Join-Path $releaseRoot "package\RenderSocoDoc_${releaseVersion}_64"
Copy-Item -LiteralPath $release64 -Destination $releasePackage -Recurse
$releaseZip = "$releasePackage.zip"
[IO.Compression.ZipFile]::CreateFromDirectory($releasePackage, $releaseZip, [IO.Compression.CompressionLevel]::Optimal, $true)
$releaseArchive = [IO.Compression.ZipFile]::OpenRead($releaseZip)
$releaseHasher = [Security.Cryptography.SHA256]::Create()
try {
    foreach ($releaseEntry in $releaseArchive.Entries) {
        if ($releaseEntry.FullName -match '(?i)(\.(pdb|lib|exp|ilk|ipdb|iobj|obj|metagen)$|(^|/)(obj|pymodules|verify[^/]*|kernel_inject)/|test_inject|renderdoc_1\.dll|\.stale|\.bak)') {
            throw "Forbidden package entry: $($releaseEntry.FullName)"
        }
        if ($releaseEntry.Name -eq '') { continue }
        $releaseStream = $releaseEntry.Open()
        try {
            $releaseHash = [BitConverter]::ToString($releaseHasher.ComputeHash($releaseStream)).Replace('-', '')
            $releaseRelative = $releaseEntry.FullName.Substring($releaseEntry.FullName.IndexOf('/') + 1)
            $releaseOriginal = Join-Path $releasePackage $releaseRelative
            if ($releaseHash -ne (Get-ReleaseHash $releaseOriginal)) {
                throw "ZIP content mismatch: $($releaseEntry.FullName)"
            }
        } finally { $releaseStream.Dispose() }
    }
} finally { $releaseHasher.Dispose(); $releaseArchive.Dispose() }
Write-Host "Validated package: $releaseZip"
Write-Host "Commit: $releaseCommit; bytes: $((Get-Item $releaseZip).Length); SHA256: $(Get-ReleaseHash $releaseZip)"
