#!/bin/bash

set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  scripts/mac_notarize_dmg.sh [options]

Options:
  --app <path>         Path to .app bundle. Default: build/PlatypusGui.app
  --dmg <path>         Path to output .dmg. Default: build/PlatypusGui.dmg
  --volname <name>     DMG volume name. Default: Platypus
  --api-key <path>     Path to App Store Connect API key .p8 file
  --key-id <id>        App Store Connect API key id
  --issuer <uuid>      App Store Connect issuer id
  --skip-dmg           Skip DMG creation and notarize an existing DMG path
  -h, --help           Show this help text

Environment:
  If a .env file exists in the repo root, it is loaded automatically.

  Supported .env variables:
    APPLE_SIGNING_IDENTITY
    APPSTORE_CONNECT_API_KEY_PATH
    APPSTORE_CONNECT_API_KEY_ID
    APPSTORE_CONNECT_API_ISSUER_ID
    PLATYPUS_APP_PATH
    PLATYPUS_DMG_PATH
    PLATYPUS_DMG_VOLNAME

Example:
  scripts/mac_notarize_dmg.sh

  scripts/mac_notarize_dmg.sh \
    --app build/PlatypusGui.app \
    --dmg build/PlatypusGui.dmg
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
API_KEY_PATH="${APPSTORE_CONNECT_API_KEY_PATH:-}"
KEY_ID="${APPSTORE_CONNECT_API_KEY_ID:-}"
ISSUER_ID="${APPSTORE_CONNECT_API_ISSUER_ID:-}"
SKIP_DMG=0

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
    --volname)
      [[ $# -ge 2 ]] || { echo "Missing value for $1" >&2; exit 1; }
      VOLNAME="$2"
      shift 2
      ;;
    --identity)
      [[ $# -ge 2 ]] || { echo "Missing value for $1" >&2; exit 1; }
      IDENTITY="$2"
      shift 2
      ;;
    --api-key)
      [[ $# -ge 2 ]] || { echo "Missing value for $1" >&2; exit 1; }
      API_KEY_PATH="$2"
      shift 2
      ;;
    --key-id)
      [[ $# -ge 2 ]] || { echo "Missing value for $1" >&2; exit 1; }
      KEY_ID="$2"
      shift 2
      ;;
    --issuer)
      [[ $# -ge 2 ]] || { echo "Missing value for $1" >&2; exit 1; }
      ISSUER_ID="$2"
      shift 2
      ;;
    --skip-dmg)
      SKIP_DMG=1
      shift
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

if [[ "${SKIP_DMG}" -eq 0 && ! -d "${APP_PATH}" ]]; then
  echo "App bundle not found: ${APP_PATH}" >&2
  exit 1
fi

if [[ -z "${API_KEY_PATH}" || -z "${KEY_ID}" || -z "${ISSUER_ID}" ]]; then
  echo "Missing notarization credentials. Set them in .env or pass --api-key, --key-id, and --issuer." >&2
  exit 1
fi

if [[ ! -f "${API_KEY_PATH}" ]]; then
  echo "App Store Connect API key not found: ${API_KEY_PATH}" >&2
  exit 1
fi

mkdir -p "$(dirname "${DMG_PATH}")"

if [[ "${SKIP_DMG}" -eq 0 ]]; then
  if [[ -z "${IDENTITY}" ]]; then
    echo "Missing DMG signing identity. Set APPLE_SIGNING_IDENTITY or pass --identity." >&2
    exit 1
  fi

  bash "${SCRIPT_DIR}/mac_package_dmg.sh" \
    --app "${APP_PATH}" \
    --dmg "${DMG_PATH}" \
    --identity "${IDENTITY}" \
    --volname "${VOLNAME}"
fi

if [[ ! -f "${DMG_PATH}" ]]; then
  echo "DMG not found: ${DMG_PATH}" >&2
  exit 1
fi

SUBMIT_JSON="$(mktemp "${TMPDIR:-/tmp}/platypus-notary-submit.XXXXXX.json")"
LOG_JSON="$(mktemp "${TMPDIR:-/tmp}/platypus-notary-log.XXXXXX.json")"
trap 'rm -f "${SUBMIT_JSON}" "${LOG_JSON}"' EXIT

echo "Submitting ${DMG_PATH} for notarization"
xcrun notarytool submit "${DMG_PATH}" \
  --key "${API_KEY_PATH}" \
  --key-id "${KEY_ID}" \
  --issuer "${ISSUER_ID}" \
  --wait \
  --output-format json > "${SUBMIT_JSON}"

cat "${SUBMIT_JSON}"

SUBMISSION_ID="$(/usr/bin/python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["id"])' "${SUBMIT_JSON}")"
SUBMISSION_STATUS="$(/usr/bin/python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["status"])' "${SUBMIT_JSON}")"

if [[ "${SUBMISSION_STATUS}" != "Accepted" ]]; then
  echo "Notarization failed with status ${SUBMISSION_STATUS}. Fetching Apple log." >&2
  xcrun notarytool log "${SUBMISSION_ID}" "${LOG_JSON}" \
    --key "${API_KEY_PATH}" \
    --key-id "${KEY_ID}" \
    --issuer "${ISSUER_ID}" || true
  if [[ -s "${LOG_JSON}" ]]; then
    cat "${LOG_JSON}" >&2
  fi
  exit 1
fi

echo "Stapling ${DMG_PATH}"
xcrun stapler staple "${DMG_PATH}"

echo "Verifying notarized DMG"
spctl -a -t open --context context:primary-signature -v "${DMG_PATH}"

echo "Done"
