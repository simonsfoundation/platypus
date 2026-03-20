# Publishing Releases with `gh`

This document covers how to publish Platypus releases from the command line
using the GitHub CLI.

## Prerequisites

1. Install and authenticate `gh`:

```bash
gh auth login
gh auth status
```

2. Make sure you are operating in the `simonsfoundation/platypus` repository:

```bash
gh repo set-default simonsfoundation/platypus
```

3. Confirm that the GitHub Actions environments and signing secrets are already
   configured. The release workflows depend on repository and environment
   secrets for signing and notarization.

## macOS Release

The macOS release workflow is [`release-macos.yml`](../.github/workflows/release-macos.yml).
It is a `workflow_dispatch` workflow and expects:

- `tag`
- `target_ref`
- `prerelease`

Run it with:

```bash
gh workflow run release-macos.yml \
  --ref main \
  -f tag=v0.1.0 \
  -f target_ref=main \
  -f prerelease=false
```

For a prerelease from a branch or commit:

```bash
gh workflow run release-macos.yml \
  --ref main \
  -f tag=v0.1.0-rc1 \
  -f target_ref=<branch-or-sha> \
  -f prerelease=true
```

Watch the run:

```bash
gh run list --workflow release-macos.yml --limit 5
gh run watch <run-id>
```

Inspect the resulting release:

```bash
gh release view v0.1.0
gh release download v0.1.0 --dir /tmp/platypus-release-check
```

## macOS 13 Compatibility Release

The macOS 13 compatibility workflow is
[`release-macos13.yml`](../.github/workflows/release-macos13.yml).
It is a separate `workflow_dispatch` workflow that builds macOS 13-compatible
artifacts on current GitHub macOS runners using the repo's `vcpkg` manifest
path instead of Homebrew `qt` and `opencv`.

This workflow does not create the GitHub Release. Run the normal macOS release
workflow first so the tag and release already exist, then add the compatibility
assets with:

```bash
gh workflow run release-macos13.yml \
  --ref main \
  -f tag=v0.1.0
```

Watch the run:

```bash
gh run list --workflow release-macos13.yml --limit 5
gh run watch <run-id>
```

The compatibility workflow uploads these extra assets to the existing release:

- `Platypus-macos13-arm64.dmg`
- `Platypus-macos13-arm64.sha256`
- `Platypus-macos13-x86_64.dmg`
- `Platypus-macos13-x86_64.sha256`
- `Platypus-photoshop-macos13-arm64.dmg`
- `Platypus-photoshop-macos13-arm64.sha256`
- `Platypus-photoshop-macos13-x86_64.dmg`
- `Platypus-photoshop-macos13-x86_64.sha256`

## Windows Release

The Windows workflow is
[`build-and-release-windows.yml`](../.github/workflows/build-and-release-windows.yml).
It runs on:

- `push` to tags matching `v*`
- `workflow_dispatch`

The publish step in that workflow is gated on `github.ref` being a tag ref, so
for a release you should run it against the tag you want to publish.

Create and push the tag if it does not already exist:

```bash
git tag v0.1.0
git push origin v0.1.0
```

That tag push will trigger the workflow automatically.

If you need to rerun it manually against an existing tag:

```bash
gh workflow run build-and-release-windows.yml --ref v0.1.0
```

Watch the run:

```bash
gh run list --workflow build-and-release-windows.yml --limit 5
gh run watch <run-id>
```

Inspect the resulting release:

```bash
gh release view v0.1.0
gh release download v0.1.0 --dir /tmp/platypus-release-check
```

## Current Limitation

The current workflows are not coordinated around a shared GitHub Release.

- The macOS workflow creates the GitHub Release itself.
- The Windows workflow also calls `gh release create`.

That means both workflows cannot successfully be the first publisher for the
same tag. The second workflow to reach its publish step will fail because the
release already exists.

As the repo stands today, treat one workflow as the release creator for a given
tag and expect the other workflow to need follow-up changes before both can
publish cleanly to the same release automatically.

## Useful `gh` Commands

List recent runs:

```bash
gh run list --limit 10
```

View a run summary:

```bash
gh run view <run-id> --log
```

Upload an extra asset manually:

```bash
gh release upload v0.1.0 path/to/artifact --clobber
```
