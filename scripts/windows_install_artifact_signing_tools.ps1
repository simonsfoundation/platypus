Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$downloadUri = "https://download.microsoft.com/download/70ad2c3b-761f-4aa9-a9de-e7405aa2b4c1/ArtifactSigningClientTools.msi"
$tempMsi = Join-Path $env:TEMP "ArtifactSigningClientTools.msi"

Write-Host "Downloading Artifact Signing Client Tools MSI..."
Invoke-WebRequest -Uri $downloadUri -OutFile $tempMsi

try {
  Write-Host "Installing Artifact Signing Client Tools..."
  Start-Process msiexec.exe -Wait -ArgumentList "/I `"$tempMsi`" /quiet /norestart"
} finally {
  if (Test-Path $tempMsi) {
    Remove-Item $tempMsi -Force
  }
}

Write-Host "Artifact Signing Client Tools installation complete."
