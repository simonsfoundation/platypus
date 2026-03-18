# Platypus Windows Build, Packaging, and Signing Guide

This guide explains how to build Platypus on Windows, create a self-contained installer `.exe`, sign it with Microsoft Trusted Signing, and verify that another Windows user can install and launch the app without manually installing dependencies.

The goal is a signed Windows installer that users can:

1. download from GitHub Releases
2. double-click
3. install with the default options
4. launch from the Start Menu

They should not need Qt, OpenCV, Visual Studio runtimes, CMake, or any other developer tooling on their own machine.

## What You Are Building

The Windows shipping artifact for this repo is:

1. a Release build of `PlatypusGui.exe`
2. staged with all runtime dependencies via `windeployqt` and the vcpkg runtime DLLs
3. packaged into a per-user NSIS installer `.exe`
4. signed with Microsoft Trusted Signing

This repo does not target a true single-file portable application executable for public Windows distribution. The supported user-facing format is a signed installer `.exe`.

## Repo Paths Used By This Workflow

- build helper: `configure.bat`
- installer/signing helper: `scripts/windows_package.ps1`
- signing helper: `scripts/windows_sign.ps1`
- Trusted Signing metadata sample: `scripts/windows_sign_metadata.sample.json`
- Windows CI/release workflow: `.github/workflows/build-and-release-windows.yml`

## Prerequisites

Before starting on a Windows machine, make sure you have:

- Windows 10 1809+ or Windows 11
- Visual Studio 2022 with the C++ workload
- Git
- CMake
- Ninja
- NSIS 3.03+
- Azure CLI
- permission to sign with the Platypus Trusted Signing certificate profile

You also need a valid Microsoft Trusted Signing setup:

- Artifact Signing account
- certificate profile
- signer role on that certificate profile
- the correct regional endpoint for the signing account

## One-Time Local Setup

If you keep local developer credentials or helper environment variables in a local `.venv`, activate it before running the commands below.

Typical PowerShell activation:

```powershell
. .\.venv\Scripts\Activate.ps1
```

### 1. Install build prerequisites

Make sure `git`, `cmake`, and `ninja` are on `PATH`.

Verify:

```powershell
git --version
cmake --version
ninja --version
```

### 2. Install NSIS

Install NSIS 3.03 or newer and ensure `makensis.exe` is on `PATH`.

Verify:

```powershell
makensis /VERSION
```

### 3. Install Microsoft Trusted Signing client tools

From an elevated PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\windows_install_artifact_signing_tools.ps1
```

This installs the client-side pieces needed for local signing, including the Azure Code Signing dlib used by `signtool`.

### 4. Sign in to Azure locally

This repo's local signing flow is designed around Azure CLI authentication.

```powershell
az login
```

If your org requires a specific tenant:

```powershell
az login --tenant <tenant-id>
```

### 5. Create your local Trusted Signing metadata file

Create a local copy from the sample:

```powershell
Copy-Item scripts\windows_sign_metadata.sample.json scripts\windows_sign_metadata.local.json
```

Then edit `scripts\windows_sign_metadata.local.json` and replace:

- `Endpoint`
- `CodeSigningAccountName`
- `CertificateProfileName`

The default sample is biased toward Azure CLI authentication by excluding the other `DefaultAzureCredential` providers.

## Step 1: Configure the Windows Build

From a Visual Studio Developer Command Prompt at the repo root:

```bat
configure.bat
```

This configures the repo against the checked-out `vcpkg` toolchain and prepares the CMake build tree in `build\`.

## Step 2: Build and Package an Unsigned Installer

To build the app and package the installer without signing:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\windows_package.ps1 `
  -BuildDir build `
  -Configuration Release `
  -PackageDir build\package
```

The script does all of the following:

- builds `PlatypusGui`
- stages the install tree
- runs `windeployqt --compiler-runtime`
- copies the vcpkg runtime DLLs
- runs CPack NSIS
- writes a `.sha256` checksum file for the installer

Expected output:

- installer: `build\package\Platypus-<version>-windows-x64-setup.exe`
- checksum: `build\package\Platypus-<version>-windows-x64-setup.exe.sha256`

## Step 3: Build and Package a Signed Installer

Once your local metadata file is configured and `az login` has succeeded:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\windows_package.ps1 `
  -BuildDir build `
  -Configuration Release `
  -PackageDir build\package `
  -Sign `
  -SigningMetadataPath scripts\windows_sign_metadata.local.json
```

When `-Sign` is enabled, the script signs:

1. `build\install\PlatypusGui.exe`
2. the final NSIS installer `.exe`

It also verifies each signature immediately after signing.

## Step 4: Verify the Signed Artifacts

### Basic Authenticode verification

```powershell
Get-AuthenticodeSignature build\install\PlatypusGui.exe | Format-List
Get-AuthenticodeSignature build\package\Platypus-<version>-windows-x64-setup.exe | Format-List
```

### SignTool verification

```powershell
signtool verify /pa /v build\install\PlatypusGui.exe
signtool verify /pa /v build\package\Platypus-<version>-windows-x64-setup.exe
```

## Step 5: Smoke Test the Installer

Use a clean Windows machine or VM with no developer tools installed.

Verify the following:

1. download the installer `.exe`
2. double-click it
3. install with default options
4. launch Platypus from the Start Menu
5. no missing DLL, Qt plugin, or runtime errors appear
6. `Help` opens `user_guide.pdf`
7. uninstall works from Windows Apps/uninstall flow

## CI and GitHub Release Setup

The repo workflow `.github/workflows/build-and-release-windows.yml` assumes these GitHub repository variables exist:

- `AZURE_CLIENT_ID`
- `AZURE_TENANT_ID`
- `AZURE_SUBSCRIPTION_ID`
- `TRUSTED_SIGNING_ENDPOINT`
- `TRUSTED_SIGNING_ACCOUNT_NAME`
- `TRUSTED_SIGNING_CERTIFICATE_PROFILE_NAME`

The workflow uses:

- `azure/login@v2` for Azure authentication
- `scripts/windows_package.ps1` for build, packaging, and signing
- tag pushes like `v1.2.3` to create a signed GitHub Release

## Common Failure Modes

### `makensis.exe` not found

NSIS is not installed or not on `PATH`.

Fix:

- install NSIS 3.03+
- open a new shell
- verify with `makensis /VERSION`

### `windeployqt.exe` not found

The Qt tools were not installed by the vcpkg manifest build or the build tree is incomplete.

Fix:

1. rerun `configure.bat`
2. rebuild the project
3. confirm `vcpkg_installed\x64-windows\tools\qttools\bin\windeployqt.exe` exists

### `Azure.CodeSigning.Dlib.dll` not found

Artifact Signing Client Tools are not installed, or the install failed.

Fix:

1. rerun `scripts/windows_install_artifact_signing_tools.ps1`
2. make sure the install completed successfully
3. rerun the signing step

### signing fails with authentication errors

Your local machine is not authenticated to Azure in a way `DefaultAzureCredential` can use, or the metadata points at the wrong account/profile/endpoint.

Fix:

1. rerun `az login`
2. verify the endpoint region matches the actual Trusted Signing account region
3. verify your signer role assignment on the certificate profile
4. verify the metadata JSON values

### installer launches but app fails on another machine

This usually means the staged install tree was incomplete before NSIS packaged it.

Fix:

1. rerun `scripts/windows_package.ps1`
2. make sure `windeployqt` completed successfully
3. confirm the packaged install tree contains the Qt plugin directories and runtime DLLs
4. retest on a clean VM
