# Platypus macOS Build, Signing, and Notarization Guide

This guide explains how to build a macOS `.app` for Platypus, sign it with Apple Developer credentials, notarize it, package it as a `.dmg`, and verify that other users can install it on their own Macs.

The goal is a signed, notarized drag-and-drop Mac app that users can download and open without needing Xcode, Homebrew, Qt, OpenCV, or any other developer tools.

This document is repo-specific. It assumes:

- Repository root: `platypus/`
- Apple ID: stored locally in your activated `.venv` as `APPLE_ID`
- Team ID: stored locally in your activated `.venv` as `APPLE_TEAM_ID`
- Bundle ID to use for the app: `com.simonsfoundation.platypus`
- Signing certificate: stored locally in your activated `.venv` as `APPLE_SIGNING_IDENTITY`
- helper signing script: `scripts/mac_sign_app.sh`

Before running the commands below, activate your local `.venv` so the Apple signing variables are available in the shell:

```bash
source .venv/bin/activate
```

## What You Are Building

For direct macOS distribution outside the Mac App Store, the normal shipping format is:

1. a signed `.app` bundle
2. packaged inside a signed and notarized `.dmg`

Users then:

1. download the `.dmg`
2. open it
3. drag `PlatypusGui.app` into `/Applications`
4. launch the app normally

You do not need a `.pkg` installer for this workflow. A `.pkg` is only necessary if you need installer behavior such as privileged installation, scripts, launch daemons, or writing files into multiple system locations.

## Apple Requirements

For a Mac app distributed outside the Mac App Store, Apple expects:

- a `Developer ID Application` certificate
- code signing with hardened runtime
- notarization through Apple's notary service
- stapling the notarization ticket to the distributed artifact

This repo already has the start of a macOS bundle path through `BUILD_APPLE_APP` and `macdeployqt`.

## Prerequisites

Before starting, make sure you have:

- a Mac
- an Apple Developer Program membership
- Xcode command line tools
- `cmake`
- `ninja`
- a C++ compiler
- Qt 6 tools, including `macdeployqt`
- either system OpenCV/Qt6 or the ability to build them through `vcpkg`

Useful checks:

```bash
xcode-select -p
cmake --version
ninja --version
which macdeployqt
```

Expected for `macdeployqt` on this machine:

```bash
/opt/homebrew/bin/macdeployqt
```

## Step 1: Use the Correct Apple Certificate

For a downloadable `.app`, the correct certificate type is:

- `Developer ID Application`

Do not use:

- `Developer ID Installer` for signing the app itself
- `Mac App Distribution` unless shipping through the Mac App Store
- `Apple Development` or `Mac Development` for public distribution

`Developer ID Installer` is only for signing a `.pkg` installer. It cannot be used to sign the `.app` bundle you want users to drag into `/Applications`.

You already verified the correct certificate file:

```text
$APPLE_SIGNING_IDENTITY
```

## Step 2: Install the Certificate into Keychain

If the certificate file is not yet installed:

1. Double-click the `.cer` file in Finder.
2. Allow it to be added to Keychain Access.
3. Make sure it appears in the `login` keychain.

Verify from Terminal:

```bash
security find-identity -v -p codesigning
```

Expected output should include:

```text
$APPLE_SIGNING_IDENTITY
```

Example from this machine:

```text
1) ABCDEF1234567890ABCDEF1234567890ABCDEF12 "$APPLE_SIGNING_IDENTITY"
   1 valid identities found
```

If you do not see that identity, stop and fix the certificate installation before continuing.

If `security find-identity -v -p codesigning` shows only a `Developer ID Installer` identity and not a `Developer ID Application` identity, you have the wrong certificate for this workflow.

## Step 3: Understand the Keychain Password Prompt

When `codesign` or `macdeployqt` accesses the private key, macOS may show a popup like:

```text
codesign wants to sign using key "Your Name" in your keychain.
To allow this, enter the "login" keychain password.
```

In most setups, the `login` keychain password is your normal macOS account password.

This prompt is local to your Mac. It is not asking for:

- your Apple ID password
- your Apple Developer portal password
- your app-specific password for notarization

If the dialog appears:

1. enter your normal Mac login password
2. allow access

If the password does not work, your `login` keychain password may be out of sync with your macOS account password. Fix that in Keychain Access before proceeding.

If you are repeatedly prompted during signing, you may need to grant `codesign` access to the private key in Keychain Access.

## Step 4: Update the App Bundle Metadata in the Repo

Before shipping a real app, the bundle metadata in the repo must be correct.

### Required change: bundle identifier

The current `MACOSX_FRAMEWORK_IDENTIFIER` in `CMakeLists.txt` is still a placeholder. It must be changed to the real bundle id:

```text
com.simonsfoundation.platypus
```

The relevant section is the `BUILD_APPLE_APP` bundle target in `CMakeLists.txt`.

Update this property:

```cmake
MACOSX_FRAMEWORK_IDENTIFIER com.simonsfoundation.platypus
```

### Recommended change: version alignment

Make sure the app version is consistent across:

- `project(VERSION ...)` in `CMakeLists.txt`
- the runtime version in `src/main.cpp`
- release artifact names

At the time of writing, the project metadata is not fully aligned. Fix that before public release so Finder, Gatekeeper, and users see consistent version information.

## Step 5: Choose Your Architecture Strategy

You need to decide whether you are building:

- a universal binary for both Intel and Apple Silicon
- an Apple Silicon-only build
- separate Intel and Apple Silicon builds

Recommended for public release:

- universal binary

Universal build value:

```bash
-DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
```

If you only build `arm64`, Intel Macs will not be able to run the app.

## Step 6: Configure the Build

From the repo root, configure the app bundle build.

### Recommended universal build

```bash
./configure.sh \
  -DBUILD_APPLE_APP=ON \
  -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
```

### Simpler local Apple Silicon-only build

```bash
./configure.sh -DBUILD_APPLE_APP=ON
```

What `./configure.sh` does:

- detects your platform
- checks for `cmake`
- checks for `ninja`
- probes for system Qt6 and OpenCV
- falls back to local `vcpkg/` if necessary
- configures CMake in `build/`

If dependencies are not already installed, the first run may take a long time because Qt/OpenCV may be built through `vcpkg`.

## Step 7: Build the App

Build the GUI target:

```bash
cmake --build build --target PlatypusGui
```

Expected output:

- `build/PlatypusGui.app`

You can confirm it exists:

```bash
ls -ld build/PlatypusGui.app
```

If the app bundle does not appear, re-check that you configured with `-DBUILD_APPLE_APP=ON`.

## Step 8: Run the Unsigned or Partially Signed App Locally

Before deployment, it is useful to verify that the app at least launches locally:

```bash
open build/PlatypusGui.app
```

Or run the Mach-O directly:

```bash
./build/PlatypusGui.app/Contents/MacOS/PlatypusGui
```

If the app does not launch at this stage, do not move on to notarization. Fix build/runtime issues first.

## Step 9: Bundle Qt Frameworks and Plugins

Use `macdeployqt` to copy Qt frameworks, plugins, and deployment metadata into the `.app`.

Recommended command:

```bash
chmod +x scripts/mac_sign_app.sh

scripts/mac_sign_app.sh \
  --identity "$APPLE_SIGNING_IDENTITY"
```

This script wraps the deployment and signing flow that worked best for this repo after `macdeployqt` surfaced invalid nested-library signatures from bundled OpenCV/VTK dependencies.

What the script does:

- runs `macdeployqt ... -always-overwrite`
- re-signs nested frameworks, bundles, dylibs, shared objects, and executables from the inside out
- signs the final `.app` bundle
- runs `codesign --verify`
- runs `spctl`
- prints the top-level `otool -L` output for a final dependency sanity check

If you prefer the raw command, the first deployment step is still:

```bash
macdeployqt build/PlatypusGui.app -always-overwrite
```

The helper script supports a few useful options:

```bash
scripts/mac_sign_app.sh --help
scripts/mac_sign_app.sh --identity "$APPLE_SIGNING_IDENTITY" --skip-deploy
scripts/mac_sign_app.sh --verify-only --app build/PlatypusGui.app
```

What it does:

- copies required Qt frameworks into the app bundle
- copies Qt plugins into the bundle
- adjusts load paths
- prepares the app for re-signing and notarization

If a keychain prompt appears during this step, enter your normal macOS account password for the `login` keychain.

### Exact failure that motivated the script

Using `macdeployqt` with direct signing:

```bash
macdeployqt build/PlatypusGui.app \
  -always-overwrite \
  -sign-for-notarization="$APPLE_SIGNING_IDENTITY"
```

produced this repo-specific failure:

```text
ERROR: codesign verification error:
ERROR: "build/PlatypusGui.app: invalid signature (code or signature have been modified)
In subcomponent: /path/to/platypus/build/PlatypusGui.app/Contents/Frameworks/libvtkImagingSources-9.5.1.dylib
In architecture: arm64"
```

That error indicates the nested third-party dylib signatures inside the bundle were no longer valid after deployment. The fix is to let `macdeployqt` deploy first, then re-sign the entire bundle from the inside out. `scripts/mac_sign_app.sh` does that.

### Manual fallback if you do not want to use the script

If you ever need to reproduce the script behavior by hand:

```bash
APP="build/PlatypusGui.app"
IDENTITY="$APPLE_SIGNING_IDENTITY"

macdeployqt "$APP" -always-overwrite

find "$APP/Contents" -type f \( -name '*.dylib' -o -name '*.so' -o -perm -111 \) -print0 | \
while IFS= read -r -d '' f; do
  codesign --force --sign "$IDENTITY" --options runtime --timestamp "$f"
done

codesign --force --sign "$IDENTITY" --options runtime --timestamp "$APP"
codesign --verify --deep --strict --verbose=2 "$APP"
spctl -a -t exec -vv "$APP"
```

## Step 10: Verify the App Signature

After `macdeployqt` completes, verify the app bundle.

### Basic signature verification

```bash
scripts/mac_sign_app.sh --verify-only --app build/PlatypusGui.app
```

### Gatekeeper assessment

```bash
spctl -a -t exec -vv build/PlatypusGui.app
```

### Inspect linked libraries

```bash
otool -L build/PlatypusGui.app/Contents/MacOS/PlatypusGui
```

What you want:

- `codesign` succeeds
- `spctl` shows a valid Developer ID signature
- `otool -L` does not point at random local dependency paths that will not exist on user machines

### Important note about OpenCV and non-Qt libraries

`macdeployqt` handles Qt dependencies. It may not fully handle non-Qt libraries such as OpenCV.

This repo links OpenCV, so inspect the `otool -L` output carefully. If you still see references to:

- Homebrew paths
- `vcpkg_installed/...`
- other local absolute paths

then the app is not yet self-contained. Those libraries must also be copied into the bundle and have their install names fixed before release.

In this repo, OpenCV currently pulls in a very large dependency graph, including many VTK dylibs. That makes the `.app` bundle larger and makes signing more fragile. If you can reduce the OpenCV modules linked by the app in the future, macOS packaging will become simpler and more reliable.

Do not skip this check. A locally working app can still fail on another user's machine if it depends on libraries only present on your system.

### Why the helper script exists

This repo currently pulls in a large OpenCV dependency graph, including VTK dylibs. After `macdeployqt` rewrites and bundles those third-party libraries, some nested signatures may become invalid. The helper script re-signs the entire bundle from the inside out so the final `.app` verifies cleanly.

## Step 11: Store Notarization Credentials

For local notarization, store a reusable notarytool profile in your keychain.

You will need:

- Apple ID: `$APPLE_ID`
- Team ID: `$APPLE_TEAM_ID`
- an app-specific password created from your Apple account

Create the stored profile:

```bash
xcrun notarytool store-credentials "platypus-notary" \
  --apple-id "$APPLE_ID" \
  --team-id "$APPLE_TEAM_ID" \
  --password "APP_SPECIFIC_PASSWORD"
```

Do not put your real app-specific password into documentation, scripts committed to git, or shell history you intend to share.

If successful, the credential profile `platypus-notary` will be stored locally in your keychain.

## Step 12: Create a DMG

The normal public artifact for this app should be a `.dmg`.

From the repo root:

```bash
macdeployqt build/PlatypusGui.app \
  -dmg \
  -sign-for-notarization="$APPLE_SIGNING_IDENTITY"
```

This should create:

- `build/PlatypusGui.dmg`

Confirm:

```bash
ls -lh build/PlatypusGui.dmg
```

## Step 13: Submit the DMG for Notarization

Use `notarytool`, not `altool`.

Submit and wait:

```bash
xcrun notarytool submit build/PlatypusGui.dmg \
  --keychain-profile "platypus-notary" \
  --wait
```

If notarization succeeds, Apple will return an accepted result.

If notarization fails, inspect the log:

```bash
xcrun notarytool log <SUBMISSION_ID> \
  --keychain-profile "platypus-notary"
```

Common failure causes:

- unsigned nested code
- invalid entitlements
- dependencies outside the bundle
- stale or invalid signatures
- a binary modified after signing

## Step 14: Staple the Notarization Ticket

After successful notarization, staple the ticket to the DMG:

```bash
xcrun stapler staple build/PlatypusGui.dmg
```

You can also staple the `.app` if desired, but the distributed DMG is the important artifact.

## Step 15: Verify the Final Downloadable Artifact

Run Gatekeeper validation on the stapled DMG:

```bash
spctl -a -t open --context context:primary-signature -v build/PlatypusGui.dmg
```

Recommended manual test:

1. copy the DMG to a different Mac if possible
2. open the DMG
3. drag `PlatypusGui.app` to `/Applications`
4. launch it
5. verify it opens without missing-framework errors

This is the real installation test. Passing notarization is necessary, but user install success still depends on the bundle containing all required runtime dependencies.

## Suggested Full Command Sequence

From a clean repo root:

```bash
chmod +x scripts/mac_sign_app.sh

./configure.sh \
  -DBUILD_APPLE_APP=ON \
  -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"

cmake --build build --target PlatypusGui

scripts/mac_sign_app.sh \
  --identity "$APPLE_SIGNING_IDENTITY"

scripts/mac_sign_app.sh --verify-only --app build/PlatypusGui.app

xcrun notarytool store-credentials "platypus-notary" \
  --apple-id "$APPLE_ID" \
  --team-id "$APPLE_TEAM_ID" \
  --password "APP_SPECIFIC_PASSWORD"

macdeployqt build/PlatypusGui.app \
  -dmg \
  -sign-for-notarization="$APPLE_SIGNING_IDENTITY"

xcrun notarytool submit build/PlatypusGui.dmg \
  --keychain-profile "platypus-notary" \
  --wait

xcrun stapler staple build/PlatypusGui.dmg
spctl -a -t open --context context:primary-signature -v build/PlatypusGui.dmg
```

## Troubleshooting

### Problem: `security find-identity` does not show the Developer ID Application cert

Cause:

- certificate not installed correctly
- private key missing
- wrong certificate type created

Fix:

1. ensure you created `Developer ID Application`
2. install the `.cer` on the same machine or same keychain context as the CSR/private key
3. verify it appears in Keychain Access and in `security find-identity -v -p codesigning`

### Problem: keychain prompt appears during signing

Cause:

- normal macOS behavior when a process accesses the private key

Fix:

- enter your normal macOS account password for the `login` keychain

### Problem: app runs on your machine but fails on another Mac

Cause:

- one or more dependencies are still outside the bundle

Fix:

1. inspect `otool -L`
2. copy missing libraries into the app bundle
3. update install names
4. re-sign the bundle
5. notarize again

### Problem: `macdeployqt` reports `invalid signature (code or signature have been modified)`

Cause:

- bundled third-party dylibs such as OpenCV or VTK were modified during deployment, leaving nested signatures stale

Fix:

1. do not rely on `macdeployqt -sign-for-notarization` alone for this repo
2. run:

```bash
chmod +x scripts/mac_sign_app.sh

scripts/mac_sign_app.sh \
  --identity "$APPLE_SIGNING_IDENTITY"
```

3. if needed, rerun verification only:

```bash
scripts/mac_sign_app.sh --verify-only --app build/PlatypusGui.app
```

### Problem: notarization fails

Cause:

- invalid nested code signatures
- missing hardened runtime
- unbundled dependencies
- files changed after signing

Fix:

1. fetch the notary log
2. identify the offending path
3. re-bundle or re-sign
4. submit again

### Problem: Intel Macs cannot run the app

Cause:

- build was created as `arm64` only

Fix:

- rebuild with:

```bash
-DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
```

### Problem: `macdeployqt` is not found

Cause:

- Qt tools are not installed or not on `PATH`

Fix:

1. confirm it exists:

```bash
which macdeployqt
```

2. if needed, pass the binary path explicitly to the helper:

```bash
scripts/mac_sign_app.sh \
  --identity "$APPLE_SIGNING_IDENTITY" \
  --macdeployqt /opt/homebrew/bin/macdeployqt
```

## Release Checklist

Before publishing a Mac release:

- bundle id set to `com.simonsfoundation.platypus`
- version metadata aligned
- app builds successfully
- app launches locally
- `macdeployqt` completed successfully
- `codesign --verify` passes
- `spctl` passes for the `.app`
- `otool -L` shows a self-contained app
- helper script `scripts/mac_sign_app.sh` completes successfully
- DMG created
- notarization accepted
- DMG stapled
- final DMG tested on a second Mac if possible

## Official References

- Apple Developer ID overview  
  https://developer.apple.com/support/developer-id/
- Apple macOS distribution overview  
  https://developer.apple.com/macos/distribution/
- Apple notarization workflow  
  https://developer.apple.com/documentation/security/notarizing_macos_software_before_distribution
- Qt macOS deployment  
  https://doc.qt.io/qt-6/macos-deployment.html
