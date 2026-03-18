param(
  [string]$BuildDir = "build",
  [string]$Configuration = "Release",
  [string]$InstallDir,
  [string]$PackageDir,
  [switch]$SkipBuild,
  [switch]$Sign,
  [string]$SigningMetadataPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-ExistingPath {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Path
  )

  $resolved = Resolve-Path -LiteralPath $Path -ErrorAction SilentlyContinue
  if (-not $resolved) {
    throw "Path not found: $Path"
  }

  return $resolved.Path
}

function Find-FirstFile {
  param(
    [Parameter(Mandatory = $true)]
    [string[]]$Candidates
  )

  foreach ($candidate in $Candidates) {
    if (Test-Path $candidate) {
      return (Resolve-ExistingPath -Path $candidate)
    }
  }

  throw "Unable to locate any of: $($Candidates -join ', ')"
}

$repoRoot = Resolve-ExistingPath -Path (Join-Path $PSScriptRoot "..")
$resolvedBuildDir = Join-Path $repoRoot $BuildDir
$resolvedInstallDir = if ($InstallDir) { Join-Path $repoRoot $InstallDir } else { Join-Path $resolvedBuildDir "install" }
$resolvedPackageDir = if ($PackageDir) { Join-Path $repoRoot $PackageDir } else { Join-Path $resolvedBuildDir "package" }

$cpackConfig = Join-Path $resolvedBuildDir "CPackConfig.cmake"
$signScript = Join-Path $PSScriptRoot "windows_sign.ps1"
$qtDeploy = Find-FirstFile -Candidates @(
  (Join-Path $repoRoot "vcpkg_installed\x64-windows\tools\qttools\bin\windeployqt.exe"),
  (Join-Path $resolvedBuildDir "vcpkg_installed\x64-windows\tools\qttools\bin\windeployqt.exe")
)
$runtimeDllDir = Find-FirstFile -Candidates @(
  (Join-Path $repoRoot "vcpkg_installed\x64-windows\bin"),
  (Join-Path $resolvedBuildDir "vcpkg_installed\x64-windows\bin")
)

if (-not $SkipBuild) {
  & cmake --build $resolvedBuildDir --config $Configuration --target PlatypusGui
  if ($LASTEXITCODE -ne 0) {
    throw "CMake build failed."
  }
}

if (-not (Test-Path $cpackConfig)) {
  throw "Missing $cpackConfig. Configure the build directory before packaging."
}

if (-not (Get-Command makensis.exe -ErrorAction SilentlyContinue)) {
  throw "makensis.exe was not found on PATH. Install NSIS 3.03+ before packaging."
}

if (Test-Path $resolvedInstallDir) {
  Remove-Item $resolvedInstallDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $resolvedInstallDir | Out-Null

& cmake --install $resolvedBuildDir --config $Configuration --prefix $resolvedInstallDir
if ($LASTEXITCODE -ne 0) {
  throw "CMake install failed."
}

$appExe = Join-Path $resolvedInstallDir "PlatypusGui.exe"
if (-not (Test-Path $appExe)) {
  throw "Expected staged application at $appExe"
}

& $qtDeploy --compiler-runtime --dir $resolvedInstallDir $appExe
if ($LASTEXITCODE -ne 0) {
  throw "windeployqt failed."
}

Get-ChildItem -Path $runtimeDllDir -Filter *.dll | ForEach-Object {
  Copy-Item $_.FullName -Destination $resolvedInstallDir -Force
}

if ($Sign) {
  if (-not $SigningMetadataPath) {
    throw "Signing was requested but no -SigningMetadataPath was provided."
  }

  & $signScript -Files $appExe -MetadataPath $SigningMetadataPath
  if ($LASTEXITCODE -ne 0) {
    throw "Local app signing failed."
  }
}

if (Test-Path $resolvedPackageDir) {
  Remove-Item $resolvedPackageDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $resolvedPackageDir | Out-Null

& cpack --config $cpackConfig -G NSIS -B $resolvedPackageDir
if ($LASTEXITCODE -ne 0) {
  throw "cpack failed."
}

$installer = Get-ChildItem -Path $resolvedPackageDir -Filter *.exe |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 1
if (-not $installer) {
  throw "NSIS installer was not created."
}

if ($Sign) {
  & $signScript -Files $installer.FullName -MetadataPath $SigningMetadataPath
  if ($LASTEXITCODE -ne 0) {
    throw "Installer signing failed."
  }
}

$hash = Get-FileHash -Path $installer.FullName -Algorithm SHA256
$checksumPath = "$($installer.FullName).sha256"
$checksumLine = "$($hash.Hash.ToLowerInvariant()) *$($installer.Name)"
Set-Content -LiteralPath $checksumPath -Value $checksumLine -Encoding ascii

Write-Host "Staged app: $appExe"
Write-Host "Installer: $($installer.FullName)"
Write-Host "Checksum: $checksumPath"
