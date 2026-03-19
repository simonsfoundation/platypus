# Platypus macOS Release Guide

This guide is the repo-specific path for producing a macOS release that other people can install from a GitHub release.

The release artifact is:

1. a self-contained `PlatypusGui.app`
2. packaged inside a signed `.dmg`
3. notarized by Apple
4. stapled so Gatekeeper can validate it offline

This is the release flow that now works locally for this repo.

## Release Design

Platypus ships outside the Mac App Store. The correct packaging model for that is:

- sign the app with `Developer ID Application`
- enable hardened runtime on the signed app
- create a drag-and-drop DMG containing the app and an `/Applications` symlink
- sign the DMG
- notarize the signed DMG
- staple the DMG
- verify the DMG with `spctl`

For this repo, the app is not packaged directly from the raw build tree. The working release path is:

1. configure
2. build
3. `cmake --install` into a staging directory
4. deploy Qt into the staged `.app`
5. fix up non-Qt dylibs inside the staged `.app`
6. sign the staged `.app`
7. verify the staged `.app`
8. create and sign the `.dmg`
9. notarize and staple the `.dmg`
10. verify the final `.dmg`

That staging step matters. It keeps packaging logic off the raw build tree and produces a cleaner release bundle.

## App Metadata

The app is configured with:

- bundle id: `com.simonsfoundation.platypus`
- signing identity: `Developer ID Application: Stephen Bronder (2P8X8JDM2A)`
- Team ID: `2P8X8JDM2A`

The app version comes from CMake:

- default: `project(VERSION ...)`
- release override: `-DPLATYPUS_PACKAGE_VERSION_OVERRIDE=<version>`

The macOS bundle also embeds:

- `mac/AppIcon.icns`
- `installer/payload/files/user_guide.pdf`

The user guide is bundled into `Contents/Resources`, and the app now resolves it from there on macOS.

## Required Apple Credentials

For this workflow you need:

- a `Developer ID Application` certificate installed in your login keychain
- an App Store Connect API key `.p8` file for notarization

You do not need:

- `Developer ID Installer`
- Mac App Store distribution certificates
- App Store submission

Verify the signing identity is installed:

```bash
security find-identity -v -p codesigning
```

You should see:

```text
Developer ID Application: Stephen Bronder (2P8X8JDM2A)
```

If macOS prompts for the `login` keychain password during signing, that is usually your normal Mac login password.

## Local Tools

You need:

- Xcode command line tools
- CMake
- Ninja
- Qt 6, including `macdeployqt`
- OpenCV

Useful checks:

```bash
xcode-select -p
cmake --version
ninja --version
which macdeployqt
brew --prefix qt
brew --prefix opencv
```

The current local and CI fast path uses Homebrew Qt and OpenCV.

## Local `.env`

Local signing and notarization scripts read `.env` from the repo root.

Start from:

```bash
cp .env.example .env
```

Set these values:

```bash
APPLE_SIGNING_IDENTITY=Developer ID Application: Stephen Bronder (2P8X8JDM2A)
APPSTORE_CONNECT_API_KEY_PATH=/Users/sbronder/opensource/AuthKey_TL2LV7M6KY.p8
APPSTORE_CONNECT_API_KEY_ID=TL2LV7M6KY
APPSTORE_CONNECT_API_ISSUER_ID=31304771-7f34-489b-9c78-292422842747
PLATYPUS_APP_PATH=build/PlatypusGui.app
PLATYPUS_DMG_PATH=build/PlatypusGui.dmg
PLATYPUS_DMG_VOLNAME=Platypus
```

The helper scripts let command-line flags override `.env`, which is useful for staged release directories.

## Local Release Pipeline

### 1. Configure

The release path should use `BUILD_APPLE_APP=ON`, disable tests, and point CMake at Homebrew Qt/OpenCV.

For Apple Silicon:

```bash
QT_PREFIX="$(brew --prefix qt)"
OPENCV_PREFIX="$(brew --prefix opencv)"

cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_APPLE_APP=ON \
  -DBUILD_TESTING=OFF \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_PREFIX_PATH="${QT_PREFIX};${OPENCV_PREFIX}" \
  -DOpenCV_DIR="${OPENCV_PREFIX}/lib/cmake/opencv4"
```

For Intel, change:

```bash
-DCMAKE_OSX_ARCHITECTURES=x86_64
```

You can also set a release version string:

```bash
-DPLATYPUS_PACKAGE_VERSION_OVERRIDE=0.0.2
```

Important detail:

- CMake now fails early if `macdeployqt` cannot be found.
- If it is not on `PATH`, pass:

```bash
-DMACDEPLOYQT_EXECUTABLE=/path/to/macdeployqt
```

### 2. Build

```bash
cmake --build build --config Release --target PlatypusGui
```

The raw app bundle will exist under `build/PlatypusGui.app`, but do not package that tree directly for release.

### 3. Install to a staging directory

Create a clean staging area and install the app bundle into it:

```bash
STAGE_DIR="$(mktemp -d "${TMPDIR:-/tmp}/platypus-stage.XXXXXX")"
cmake --install build --prefix "${STAGE_DIR}"
```

Expected staged app path:

```bash
${STAGE_DIR}/PlatypusGui.app
```

During install, the repo runs two mac-specific steps:

1. `PlatypusDeployBundle.cmake`
   - runs `macdeployqt` with `-no-plugins -always-overwrite`
   - copies only the Qt plugins this app actually needs
2. `PlatypusFixBundle.cmake`
   - runs `fixup_bundle(...)`
   - copies non-Qt dylibs into the bundle
   - normalizes framework root symlinks so Qt frameworks have a valid macOS layout for signing

This is why the installed app is the canonical release bundle.

### 4. Sign the staged app

Run the signing helper on the staged app and skip deployment, since deployment already happened at install time:

```bash
scripts/mac_sign_app.sh \
  --app "${STAGE_DIR}/PlatypusGui.app" \
  --skip-deploy
```

What the script does:

- loads `.env`
- signs loose dylibs and executables outside bundle containers
- signs framework bundles
- signs plug-ins, bundles, nested apps, and XPC services
- signs the outer `.app`
- applies hardened runtime and secure timestamps

Important signing rule:

- framework and bundle containers are signed atomically
- the script intentionally avoids signing random files inside `*.framework` and `*.bundle` trees one-by-one

That avoids invalidating framework seals.

### 5. Verify the staged app before notarization

```bash
scripts/mac_verify_release.sh \
  --phase pre-notarize \
  --app "${STAGE_DIR}/PlatypusGui.app" \
  --report-dir "${STAGE_DIR}/verification/pre-notarize"
```

The verifier enforces these release rules:

- Developer ID signature present
- hardened runtime present
- no release entitlements
- nested code signatures valid
- no Homebrew, vcpkg, or other local absolute dylib references in the main executable

Note about `syspolicy_check`:

- this script runs `syspolicy_check notary-submission` only as an informational check
- on current macOS builds it can crash or produce low-signal failures even when notarization later succeeds
- it is logged as a warning, not a release blocker

### 6. Create and sign the DMG

Use the helper script:

```bash
DMG_PATH="${STAGE_DIR}/PlatypusGui.dmg"

scripts/mac_package_dmg.sh \
  --app "${STAGE_DIR}/PlatypusGui.app" \
  --dmg "${DMG_PATH}" \
  --volname "Platypus"
```

This script:

- copies the app into a temporary DMG staging directory
- creates an `/Applications` symlink
- builds a read-only compressed HFS+ DMG
- verifies the DMG structure with `hdiutil verify`
- signs the DMG with `Developer ID Application`
- verifies the DMG signature

This step is required. A stapled DMG that was never signed will fail `spctl` with:

```text
source=no usable signature
```

The correct order is:

1. sign the `.app`
2. create the `.dmg`
3. sign the `.dmg`
4. notarize the signed `.dmg`
5. staple the `.dmg`
6. verify the `.dmg`

### 7. Notarize and staple the DMG

Use the end-to-end helper:

```bash
scripts/mac_notarize_dmg.sh \
  --app "${STAGE_DIR}/PlatypusGui.app" \
  --dmg "${DMG_PATH}"
```

If you already created the DMG and only want to notarize it:

```bash
scripts/mac_notarize_dmg.sh \
  --skip-dmg \
  --app "${STAGE_DIR}/PlatypusGui.app" \
  --dmg "${DMG_PATH}"
```

The notarization script:

- submits the signed DMG with `xcrun notarytool submit ... --wait`
- staples the DMG
- verifies the notarized DMG with:

```bash
spctl -a -t open --context context:primary-signature -v
```

Long waits in `In Progress` are normal. Apple notarization can take several minutes.

### 8. Final post-staple verification

```bash
scripts/mac_verify_release.sh \
  --phase post-staple \
  --app "${STAGE_DIR}/PlatypusGui.app" \
  --dmg "${DMG_PATH}" \
  --report-dir "${STAGE_DIR}/verification/post-staple"
```

This verifies:

- app signature metadata
- nested code signatures
- no unexpected entitlements
- no disallowed dependency paths
- DMG signature
- stapler validation
- Gatekeeper acceptance of the DMG

## CI Release Pipeline

GitHub Actions now follows the same release model:

1. manual `workflow_dispatch`
2. one native `arm64` job on `macos-15`
3. one native `x86_64` job on `macos-15-intel`
4. Homebrew `qt` and `opencv`
5. configure, build, and `cmake --install` into a staging directory
6. sign the staged app
7. verify the staged app
8. create and sign the DMG
9. notarize and staple the DMG
10. verify the final DMG
11. publish both DMGs to one GitHub release

Current CI artifacts:

- `Platypus-macos-arm64.dmg`
- `Platypus-macos-arm64.sha256`
- `Platypus-macos-x86_64.dmg`
- `Platypus-macos-x86_64.sha256`

The workflow uses the `release-macos` environment for:

- `APPLE_SIGNING_IDENTITY`
- `APPLE_TEAM_ID`
- `APPLE_BUNDLE_ID`
- `MACOS_CERT_P12_BASE64`
- `MACOS_CERT_P12_PASSWORD`
- `APPSTORE_CONNECT_API_KEY_ID`
- `APPSTORE_CONNECT_API_ISSUER_ID`
- `APPSTORE_CONNECT_API_KEY_BASE64`

## Common Problems

### `macdeployqt` not found

Either install Qt correctly or pass the executable path explicitly:

```bash
cmake -S . -B build ... \
  -DMACDEPLOYQT_EXECUTABLE="$(command -v macdeployqt)"
```

### `codesign` asks for the login keychain password

That is the local keychain access prompt for the private signing key. In most setups it is your normal macOS login password.

### App verifies with `codesign`, but `spctl` says `Unnotarized Developer ID`

That is normal before notarization. It means the app is signed but not notarized yet. Notarize the signed DMG, staple it, and verify the DMG instead.

### `spctl` says `source=no usable signature` for the DMG

That means the DMG itself was not signed before notarization. Recreate it with `scripts/mac_package_dmg.sh`, which signs the DMG before submission.

### Notarization is slow

That is normal. Apple often leaves submissions in `In Progress` for several minutes.

### `syspolicy_check` crashes or reports a vague failure

Current macOS versions can make this tool unreliable for pre-notarization checks. In this repo it is advisory only. The release blockers are:

- signature metadata
- nested code verification
- dependency path verification
- signed DMG
- successful notarization
- successful stapling
- `spctl` acceptance of the final DMG

## Local End-to-End Example

This is the full local path in one sequence:

```bash
QT_PREFIX="$(brew --prefix qt)"
OPENCV_PREFIX="$(brew --prefix opencv)"
STAGE_DIR="$(mktemp -d "${TMPDIR:-/tmp}/platypus-stage.XXXXXX")"
DMG_PATH="${STAGE_DIR}/PlatypusGui.dmg"

cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_APPLE_APP=ON \
  -DBUILD_TESTING=OFF \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_PREFIX_PATH="${QT_PREFIX};${OPENCV_PREFIX}" \
  -DOpenCV_DIR="${OPENCV_PREFIX}/lib/cmake/opencv4"

cmake --build build --config Release --target PlatypusGui
cmake --install build --prefix "${STAGE_DIR}"

scripts/mac_sign_app.sh \
  --app "${STAGE_DIR}/PlatypusGui.app" \
  --skip-deploy

scripts/mac_verify_release.sh \
  --phase pre-notarize \
  --app "${STAGE_DIR}/PlatypusGui.app" \
  --report-dir "${STAGE_DIR}/verification/pre-notarize"

scripts/mac_package_dmg.sh \
  --app "${STAGE_DIR}/PlatypusGui.app" \
  --dmg "${DMG_PATH}" \
  --volname "Platypus"

scripts/mac_notarize_dmg.sh \
  --skip-dmg \
  --app "${STAGE_DIR}/PlatypusGui.app" \
  --dmg "${DMG_PATH}"

scripts/mac_verify_release.sh \
  --phase post-staple \
  --app "${STAGE_DIR}/PlatypusGui.app" \
  --dmg "${DMG_PATH}" \
  --report-dir "${STAGE_DIR}/verification/post-staple"
```

If all of those succeed, you have a locally validated release pipeline.
