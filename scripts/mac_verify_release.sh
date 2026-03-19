#!/bin/bash

set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  scripts/mac_verify_release.sh --phase <pre-notarize|post-staple> [options]

Options:
  --phase <phase>        Verification phase to run
  --app <path>           Path to .app bundle. Default: build/PlatypusGui.app
  --dmg <path>           Path to .dmg for post-staple checks
  --report-dir <path>    Output directory for verification logs
  -h, --help             Show this help text

Environment:
  If a .env file exists in the repo root, it is loaded automatically.

  Supported .env variables:
    PLATYPUS_APP_PATH
    PLATYPUS_DMG_PATH
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

PHASE=""
APP_PATH="${PLATYPUS_APP_PATH:-build/PlatypusGui.app}"
DMG_PATH="${PLATYPUS_DMG_PATH:-build/PlatypusGui.dmg}"
REPORT_DIR="build/mac-verification"
FAILURES=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --phase)
      [[ $# -ge 2 ]] || { echo "Missing value for $1" >&2; exit 1; }
      PHASE="$2"
      shift 2
      ;;
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
    --report-dir)
      [[ $# -ge 2 ]] || { echo "Missing value for $1" >&2; exit 1; }
      REPORT_DIR="$2"
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

if [[ "${PHASE}" != "pre-notarize" && "${PHASE}" != "post-staple" ]]; then
  echo "--phase must be pre-notarize or post-staple" >&2
  usage >&2
  exit 1
fi

cd "${REPO_ROOT}"

mkdir -p "${REPORT_DIR}"
SUMMARY_FILE="${REPORT_DIR}/summary.txt"
: > "${SUMMARY_FILE}"

run_check() {
  local name="$1"
  shift
  local log_file="${REPORT_DIR}/${name}.log"
  echo "Running ${name}" | tee -a "${SUMMARY_FILE}"
  if "$@" >"${log_file}" 2>&1; then
    echo "PASS ${name}" | tee -a "${SUMMARY_FILE}"
  else
    echo "FAIL ${name}" | tee -a "${SUMMARY_FILE}"
    cat "${log_file}" >&2
    FAILURES=$((FAILURES + 1))
  fi
}

run_optional_check() {
  local name="$1"
  shift
  local log_file="${REPORT_DIR}/${name}.log"
  echo "Running optional ${name}" | tee -a "${SUMMARY_FILE}"
  if "$@" >"${log_file}" 2>&1; then
    echo "PASS ${name}" | tee -a "${SUMMARY_FILE}"
  else
    echo "WARN ${name}" | tee -a "${SUMMARY_FILE}"
    cat "${log_file}" >&2
  fi
}

check_app_signature_metadata() {
  local log_file="${REPORT_DIR}/codesign-display.log"
  codesign -dvv "${APP_PATH}" >"${log_file}" 2>&1
  rg -q "Authority=Developer ID Application:" "${log_file}" &&
    rg -q "Runtime Version=" "${log_file}"
}

check_nested_code_signatures() {
  local log_file="${REPORT_DIR}/nested-codesign.log"
  : >"${log_file}"

  while IFS= read -r -d '' framework; do
    local target="${framework}"
    if [[ -d "${framework}/Versions/Current" ]]; then
      target="${framework}/Versions/Current"
    fi
    if ! codesign --verify --strict --verbose=2 "${target}" >>"${log_file}" 2>&1; then
      echo "Failed framework verification: ${target}" >>"${log_file}"
      return 1
    fi
  done < <(find "${APP_PATH}/Contents" -name '*.framework' -print0 | sort -z)

  while IFS= read -r -d '' file; do
    if [[ "${file}" == "${APP_PATH}/Contents/MacOS/"* ]]; then
      continue
    fi
    if ! codesign --verify --strict --verbose=2 "${file}" >>"${log_file}" 2>&1; then
      echo "Failed code signature verification: ${file}" >>"${log_file}"
      return 1
    fi
  done < <(find "${APP_PATH}/Contents" \
      \( -name '*.framework' -o -name '*.app' -o -name '*.bundle' -o -name '*.plugin' -o -name '*.xpc' \) -prune -o \
      -type f \( -name '*.dylib' -o -name '*.so' -o -perm -111 \) -print0 | sort -z)
}

check_release_entitlements() {
  local entitlements_file="${REPORT_DIR}/entitlements.xml"
  local stderr_file="${REPORT_DIR}/entitlements.stderr"

  codesign -d --entitlements - "${APP_PATH}" >"${entitlements_file}" 2>"${stderr_file}" || true

  if rg -q "<plist|<dict|<key>" "${entitlements_file}"; then
    echo "Release app unexpectedly contains entitlements:" >&2
    cat "${entitlements_file}" >&2
    return 1
  fi
}

check_dependency_paths() {
  local binary_path="${APP_PATH}/Contents/MacOS/$(basename "${APP_PATH}" .app)"
  local log_file="${REPORT_DIR}/otool-main-executable.log"
  local dep
  otool -L "${binary_path}" >"${log_file}" 2>&1

  while IFS= read -r dep; do
    [[ -n "${dep}" ]] || continue
    case "${dep}" in
      @executable_path/*|@loader_path/*|@rpath/*|/System/Library/*|/usr/lib/*)
        ;;
      /opt/homebrew/*|/usr/local/*|/Users/*|/private/*|*vcpkg*|*Cellar*)
        echo "Disallowed dependency path: ${dep}" >&2
        return 1
        ;;
    esac
  done < <(awk 'NR > 1 {print $1}' "${log_file}")
}

if [[ ! -d "${APP_PATH}" ]]; then
  echo "App bundle not found: ${APP_PATH}" >&2
  exit 1
fi

run_check app-signature check_app_signature_metadata
run_check nested-codesign check_nested_code_signatures
run_check entitlements check_release_entitlements
run_check dependency-paths check_dependency_paths

if command -v syspolicy_check >/dev/null 2>&1; then
  run_optional_check syspolicy-notary-submission syspolicy_check notary-submission "${APP_PATH}"
fi

if [[ "${PHASE}" == "post-staple" ]]; then
  if [[ ! -f "${DMG_PATH}" ]]; then
    echo "DMG not found: ${DMG_PATH}" >&2
    exit 1
  fi
  run_check dmg-codesign codesign --verify --verbose=2 "${DMG_PATH}"
  run_check dmg-stapler xcrun stapler validate "${DMG_PATH}"
  run_check dmg-spctl spctl -a -t open --context context:primary-signature -v "${DMG_PATH}"
fi

if [[ "${FAILURES}" -ne 0 ]]; then
  echo "macOS release verification failed (${FAILURES} check(s))" >&2
  exit 1
fi

echo "All macOS release verification checks passed."
