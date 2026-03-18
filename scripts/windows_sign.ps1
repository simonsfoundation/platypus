param(
  [Parameter(Mandatory = $true)]
  [string[]]$Files,

  [string]$MetadataPath = (Join-Path $PSScriptRoot "windows_sign_metadata.local.json"),
  [string]$SignToolPath,
  [string]$DlibPath,
  [string]$TimestampUrl = "http://timestamp.acs.microsoft.com",
  [string]$FileDigest = "SHA256",
  [string]$TimestampDigest = "SHA256"
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

function Find-SignTool {
  param([string]$ExplicitPath)

  if ($ExplicitPath) {
    return (Resolve-ExistingPath -Path $ExplicitPath)
  }

  if ($env:SIGNTOOL_PATH) {
    return (Resolve-ExistingPath -Path $env:SIGNTOOL_PATH)
  }

  $windowsKitsRoot = "${env:ProgramFiles(x86)}\Windows Kits\10\bin"
  if (Test-Path $windowsKitsRoot) {
    $candidate = Get-ChildItem -Path $windowsKitsRoot -Filter signtool.exe -Recurse -ErrorAction SilentlyContinue |
      Where-Object { $_.FullName -match "\\x64\\signtool\.exe$" } |
      Sort-Object FullName -Descending |
      Select-Object -First 1
    if ($candidate) {
      return $candidate.FullName
    }
  }

  throw "Unable to locate signtool.exe. Install the Windows SDK or set SIGNTOOL_PATH."
}

function Find-Dlib {
  param([string]$ExplicitPath)

  if ($ExplicitPath) {
    return (Resolve-ExistingPath -Path $ExplicitPath)
  }

  if ($env:AZURE_CODESIGN_DLIB_PATH) {
    return (Resolve-ExistingPath -Path $env:AZURE_CODESIGN_DLIB_PATH)
  }

  $roots = @(
    "${env:ProgramFiles}\Microsoft Trusted Signing Client Tools",
    "${env:ProgramFiles(x86)}\Microsoft Trusted Signing Client Tools",
    $env:ProgramFiles,
    ${env:ProgramFiles(x86)}
  ) | Where-Object { $_ -and (Test-Path $_) }

  foreach ($root in $roots) {
    $candidate = Get-ChildItem -Path $root -Filter Azure.CodeSigning.Dlib.dll -Recurse -ErrorAction SilentlyContinue |
      Where-Object { $_.FullName -match "\\x64\\Azure\.CodeSigning\.Dlib\.dll$" } |
      Select-Object -First 1
    if ($candidate) {
      return $candidate.FullName
    }
  }

  throw "Unable to locate Azure.CodeSigning.Dlib.dll. Install Artifact Signing Client Tools or set AZURE_CODESIGN_DLIB_PATH."
}

$resolvedMetadata = Resolve-ExistingPath -Path $MetadataPath
$resolvedFiles = $Files | ForEach-Object { Resolve-ExistingPath -Path $_ }
$resolvedSignTool = Find-SignTool -ExplicitPath $SignToolPath
$resolvedDlib = Find-Dlib -ExplicitPath $DlibPath

foreach ($file in $resolvedFiles) {
  Write-Host "Signing $file"
  & $resolvedSignTool sign /v /debug `
    /fd $FileDigest `
    /tr $TimestampUrl `
    /td $TimestampDigest `
    /dlib $resolvedDlib `
    /dmdf $resolvedMetadata `
    $file

  if ($LASTEXITCODE -ne 0) {
    throw "signtool failed for $file"
  }

  & $resolvedSignTool verify /pa /v $file
  if ($LASTEXITCODE -ne 0) {
    throw "signtool verification failed for $file"
  }
}
