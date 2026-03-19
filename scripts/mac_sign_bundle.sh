#!/bin/bash

set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  scripts/mac_sign_bundle.sh --bundle <path> --identity "Developer ID Application: Name (TEAMID)" [options]

Options:
  --bundle <path>            Path to a top-level .app or .plugin bundle.
  --identity <identity>      Required unless --verify-only is used.
  --verify-only              Skip signing and only inspect the existing signature.
  --gatekeeper               Run Gatekeeper assessment after codesign verification.
  -h, --help                 Show this help text.

Environment:
  If a .env file exists in the repo root, it is loaded automatically.

  Supported .env variables:
    APPLE_SIGNING_IDENTITY
    PLATYPUS_BUNDLE_PATH
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

BUNDLE_PATH="${PLATYPUS_BUNDLE_PATH:-}"
IDENTITY="${APPLE_SIGNING_IDENTITY:-}"
VERIFY_ONLY=0
RUN_GATEKEEPER=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --bundle)
      [[ $# -ge 2 ]] || { echo "Missing value for $1" >&2; exit 1; }
      BUNDLE_PATH="$2"
      shift 2
      ;;
    --identity)
      [[ $# -ge 2 ]] || { echo "Missing value for $1" >&2; exit 1; }
      IDENTITY="$2"
      shift 2
      ;;
    --verify-only)
      VERIFY_ONLY=1
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

if [[ -z "${BUNDLE_PATH}" ]]; then
  echo "--bundle is required" >&2
  usage >&2
  exit 1
fi

if [[ ! -d "${BUNDLE_PATH}" ]]; then
  echo "Bundle not found: ${BUNDLE_PATH}" >&2
  exit 1
fi

if [[ -z "${IDENTITY}" && "${VERIFY_ONLY}" -eq 0 ]]; then
  echo "--identity is required unless --verify-only is used" >&2
  usage >&2
  exit 1
fi

BUNDLE_NAME="$(basename "${BUNDLE_PATH}")"
BUNDLE_NAME="${BUNDLE_NAME%.app}"
BUNDLE_NAME="${BUNDLE_NAME%.plugin}"
BUNDLE_EXECUTABLE_PATH="${BUNDLE_PATH}/Contents/MacOS/${BUNDLE_NAME}"

sign_path() {
  local path="$1"
  echo "Signing ${path}"
  codesign --force --sign "${IDENTITY}" --options runtime --timestamp "${path}"
}

framework_sign_target() {
  local framework_path="$1"
  if [[ -d "${framework_path}/Versions/Current" ]]; then
    printf '%s\n' "${framework_path}/Versions/Current"
    return
  fi

  local version_dir
  while IFS= read -r -d '' version_dir; do
    printf '%s\n' "${version_dir}"
    return
  done < <(find "${framework_path}/Versions" -mindepth 1 -maxdepth 1 -type d ! -name Current -print0 | sort -z)

  printf '%s\n' "${framework_path}"
}

if [[ "${VERIFY_ONLY}" -eq 0 ]]; then
  bundle_prune_expr=(
    \( -name '*.framework' -o -name '*.app' -o -name '*.bundle' -o -name '*.plugin' -o -name '*.xpc' \)
    -prune
  )

  while IFS= read -r -d '' file; do
    sign_path "${file}"
  done < <(find "${BUNDLE_PATH}/Contents" "${bundle_prune_expr[@]}" -o \
      -type f \( -name '*.dylib' -o -name '*.so' \) -print0 | sort -z)

  while IFS= read -r -d '' exec_file; do
    if [[ "${exec_file}" == "${BUNDLE_EXECUTABLE_PATH}" ]]; then
      continue
    fi
    sign_path "${exec_file}"
  done < <(find "${BUNDLE_PATH}/Contents" "${bundle_prune_expr[@]}" -o \
      -type f -perm -111 ! \( -name '*.dylib' -o -name '*.so' \) -print0 | sort -z)

  while IFS= read -r -d '' framework; do
    sign_path "$(framework_sign_target "${framework}")"
  done < <(find "${BUNDLE_PATH}/Contents" -name '*.framework' -print0 | sort -z)

  while IFS= read -r -d '' bundle; do
    sign_path "${bundle}"
  done < <(find "${BUNDLE_PATH}/Contents" \( -name '*.app' -o -name '*.bundle' -o -name '*.plugin' -o -name '*.xpc' \) -print0 | sort -z)

  sign_path "${BUNDLE_PATH}"
fi

echo "Inspecting bundle signature"
codesign -dvv "${BUNDLE_PATH}" 2>&1 | sed -n '1,80p'

if [[ "${RUN_GATEKEEPER}" -eq 1 ]]; then
  echo "Running Gatekeeper assessment"
  spctl -a -t exec -vv "${BUNDLE_PATH}"
fi

if [[ -d "${BUNDLE_PATH}/Contents/MacOS" ]]; then
  while IFS= read -r -d '' binary; do
    echo "Dependencies for ${binary}"
    otool -L "${binary}" | sed -n '1,120p'
  done < <(find "${BUNDLE_PATH}/Contents/MacOS" -type f -perm -111 -print0 | sort -z)
fi

echo "Done"
