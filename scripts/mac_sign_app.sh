#!/bin/bash

set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  scripts/mac_sign_app.sh --identity "Developer ID Application: Name (TEAMID)" [options]

Options:
  --identity <identity>      Required. codesign identity to use.
  --app <path>               Path to .app bundle. Default: build/PlatypusGui.app
  --macdeployqt <path>       Path to macdeployqt. Default: resolved from PATH
  --skip-deploy              Skip the macdeployqt deployment step.
  --verify-only              Skip deployment and signing, only verify the app.
  --gatekeeper               Run Gatekeeper assessment after codesign verification.
  -h, --help                 Show this help text.

Examples:
  scripts/mac_sign_app.sh \
    --identity "Developer ID Application: Your Name (YOURTEAMID)"

  scripts/mac_sign_app.sh \
    --identity "Developer ID Application: Your Name (YOURTEAMID)" \
    --app build/PlatypusGui.app \
    --skip-deploy

Environment:
  If a .env file exists in the repo root, it is loaded automatically.

  Supported .env variables:
    APPLE_SIGNING_IDENTITY
    PLATYPUS_APP_PATH
    MACDEPLOYQT_BIN
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
APP_EXECUTABLE_PATH="${APP_PATH}/Contents/MacOS/$(basename "${APP_PATH}" .app)"
IDENTITY="${APPLE_SIGNING_IDENTITY:-}"
MACDEPLOYQT_BIN="${MACDEPLOYQT_BIN:-}"
SKIP_DEPLOY=0
VERIFY_ONLY=0
RUN_GATEKEEPER=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --identity)
      [[ $# -ge 2 ]] || { echo "Missing value for $1" >&2; exit 1; }
      IDENTITY="$2"
      shift 2
      ;;
    --app)
      [[ $# -ge 2 ]] || { echo "Missing value for $1" >&2; exit 1; }
      APP_PATH="$2"
      shift 2
      ;;
    --macdeployqt)
      [[ $# -ge 2 ]] || { echo "Missing value for $1" >&2; exit 1; }
      MACDEPLOYQT_BIN="$2"
      shift 2
      ;;
    --skip-deploy)
      SKIP_DEPLOY=1
      shift
      ;;
    --verify-only)
      VERIFY_ONLY=1
      SKIP_DEPLOY=1
      shift
      ;;
    --gatekeeper)
      RUN_GATEKEEPER=1
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

if [[ -z "$IDENTITY" && "$VERIFY_ONLY" -eq 0 ]]; then
  echo "--identity is required unless --verify-only is used" >&2
  usage >&2
  exit 1
fi

if [[ ! -d "$APP_PATH" ]]; then
  echo "App bundle not found: $APP_PATH" >&2
  exit 1
fi

cd "${REPO_ROOT}"

if [[ "$VERIFY_ONLY" -eq 0 ]]; then
  if [[ "$SKIP_DEPLOY" -eq 0 ]]; then
    if [[ -z "$MACDEPLOYQT_BIN" ]]; then
      if ! MACDEPLOYQT_BIN="$(command -v macdeployqt)"; then
        echo "macdeployqt not found on PATH" >&2
        exit 1
      fi
    fi

    echo "Running macdeployqt on $APP_PATH"
    "$MACDEPLOYQT_BIN" "$APP_PATH" -always-overwrite
  fi

  sign_path() {
    local path="$1"
    echo "Signing $path"
    codesign --force --sign "$IDENTITY" --options runtime --timestamp "$path"
  }

  bundle_prune_expr=(
    \( -name '*.framework' -o -name '*.app' -o -name '*.bundle' -o -name '*.plugin' -o -name '*.xpc' \)
    -prune
  )

  while IFS= read -r -d '' file; do
    sign_path "$file"
  done < <(find "$APP_PATH/Contents" "${bundle_prune_expr[@]}" -o \
      -type f \( -name '*.dylib' -o -name '*.so' \) -print0 | sort -z)

  while IFS= read -r -d '' exec_file; do
    if [[ "$exec_file" == "$APP_EXECUTABLE_PATH" ]]; then
      continue
    fi
    sign_path "$exec_file"
  done < <(find "$APP_PATH/Contents" "${bundle_prune_expr[@]}" -o \
      -type f -perm -111 ! \( -name '*.dylib' -o -name '*.so' \) -print0 | sort -z)

  while IFS= read -r -d '' framework; do
    sign_path "$framework"
  done < <(find "$APP_PATH/Contents" -name '*.framework' -print0 | sort -z)

  while IFS= read -r -d '' bundle; do
    sign_path "$bundle"
  done < <(find "$APP_PATH/Contents" \( -name '*.app' -o -name '*.bundle' -o -name '*.plugin' -o -name '*.xpc' \) -print0 | sort -z)

  echo "Signing app bundle $APP_PATH"
  codesign --force --sign "$IDENTITY" --options runtime --timestamp "$APP_PATH"
fi

echo "Inspecting app signature"
codesign -dvv "$APP_PATH" 2>&1 | sed -n '1,80p'

if [[ "$RUN_GATEKEEPER" -eq 1 ]]; then
  echo "Running Gatekeeper assessment"
  spctl -a -t exec -vv "$APP_PATH"
else
  echo "Skipping Gatekeeper assessment. A Developer ID app is expected to be rejected until it is notarized."
fi

echo "Top-level executable dependencies"
otool -L "$APP_PATH/Contents/MacOS/$(basename "$APP_PATH" .app)" | sed -n '1,120p'

echo "Done"
