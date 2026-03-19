#!/bin/bash

set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  scripts/mac_package_dmg.sh [options]

Options:
  --app <path>           Path to .app bundle. Default: build/PlatypusGui.app
  --dmg <path>           Path to output .dmg. Default: build/PlatypusGui.dmg
  --identity <identity>  Developer ID Application signing identity
  --volname <name>       DMG volume name. Default: Platypus
  -h, --help             Show this help text

Environment:
  If a .env file exists in the repo root, it is loaded automatically.

  Supported .env variables:
    APPLE_SIGNING_IDENTITY
    PLATYPUS_APP_PATH
    PLATYPUS_DMG_PATH
    PLATYPUS_DMG_VOLNAME
EOF
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
ENV_FILE="${REPO_ROOT}/.env"

if [[ -f "${ENV_FILE}" ]]; then
  while IFS= read -r line || [[ -n "$line" ]]; do
    [[ -n "$line" ]] || continue
    [[ "$line" =~ ^[[:space:]]*# ]] && continue
    if [[ "$line" == *=* ]]; then
      key="${line%%=*}"
      value="${line#*=}"
      key="${key#"${key%%[![:space:]]*}"}"
      key="${key%"${key##*[![:space:]]}"}"
      export "$key=$value"
    fi
  done < "${ENV_FILE}"
fi

APP_PATH="${PLATYPUS_APP_PATH:-build/PlatypusGui.app}"
DMG_PATH="${PLATYPUS_DMG_PATH:-build/PlatypusGui.dmg}"
VOLNAME="${PLATYPUS_DMG_VOLNAME:-Platypus}"
IDENTITY="${APPLE_SIGNING_IDENTITY:-}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --app)
      [[ $# -ge 2 ]] || { echo "Missing value for $1" >&2; exit 1; }
      APP_PATH="$2"
      shift 2
      ;;
    --dmg)
      [[ $# -ge 2 ]] || { echo "Missing value for $1" >&2; exit 1; }
      DMG_PATH="$2"
      shift 2
      ;;
    --identity)
      [[ $# -ge 2 ]] || { echo "Missing value for $1" >&2; exit 1; }
      IDENTITY="$2"
      shift 2
      ;;
    --volname)
      [[ $# -ge 2 ]] || { echo "Missing value for $1" >&2; exit 1; }
      VOLNAME="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

cd "${REPO_ROOT}"

if [[ ! -d "${APP_PATH}" ]]; then
  echo "App bundle not found: ${APP_PATH}" >&2
  exit 1
fi

if [[ -z "${IDENTITY}" ]]; then
  echo "Missing DMG signing identity. Set APPLE_SIGNING_IDENTITY or pass --identity." >&2
  exit 1
fi

mkdir -p "$(dirname "${DMG_PATH}")"

STAGING_DIR="$(mktemp -d "${TMPDIR:-/tmp}/platypus-dmg.XXXXXX")"
trap 'rm -rf "${STAGING_DIR}"' EXIT

cp -R "${APP_PATH}" "${STAGING_DIR}/"
ln -s /Applications "${STAGING_DIR}/Applications"

echo "Creating DMG at ${DMG_PATH}"
hdiutil create \
  -fs HFS+ \
  -volname "${VOLNAME}" \
  -srcfolder "${STAGING_DIR}" \
  -ov \
  -format UDZO \
  "${DMG_PATH}"

echo "Verifying DMG structure"
hdiutil verify "${DMG_PATH}"

echo "Signing DMG ${DMG_PATH}"
codesign --force --sign "${IDENTITY}" --timestamp "${DMG_PATH}"

echo "Verifying DMG signature"
codesign --verify --verbose=2 "${DMG_PATH}"

echo "Done"
