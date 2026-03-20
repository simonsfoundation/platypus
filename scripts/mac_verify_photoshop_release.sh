#!/bin/bash

set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  scripts/mac_verify_photoshop_release.sh --phase <pre-notarize|post-staple> --payload-dir <path> [options]

Options:
  --phase <phase>          Verification phase to run.
  --payload-dir <path>     Path to the staged Photoshop payload directory.
  --dmg <path>             Path to the DMG for post-staple checks.
  --deployment-target <version>
                           Maximum macOS minimum-version allowed in bundled Mach-O files.
  --report-dir <path>      Output directory for verification logs.
  -h, --help               Show this help text.
EOF
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
ENV_FILE="${REPO_ROOT}/.env"
source "${SCRIPT_DIR}/mac_verify_common.sh"

load_repo_env "${ENV_FILE}"

PHASE=""
PAYLOAD_DIR=""
DMG_PATH=""
DEPLOYMENT_TARGET="${PLATYPUS_MACOS_DEPLOYMENT_TARGET:-}"
REPORT_DIR="build/mac-photoshop-verification"
FAILURES=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --phase)
      [[ $# -ge 2 ]] || { echo "Missing value for $1" >&2; exit 1; }
      PHASE="$2"
      shift 2
      ;;
    --payload-dir)
      [[ $# -ge 2 ]] || { echo "Missing value for $1" >&2; exit 1; }
      PAYLOAD_DIR="$2"
      shift 2
      ;;
    --dmg)
      [[ $# -ge 2 ]] || { echo "Missing value for $1" >&2; exit 1; }
      DMG_PATH="$2"
      shift 2
      ;;
    --deployment-target)
      [[ $# -ge 2 ]] || { echo "Missing value for $1" >&2; exit 1; }
      DEPLOYMENT_TARGET="$2"
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

if [[ -z "${PAYLOAD_DIR}" ]]; then
  echo "--payload-dir is required" >&2
  usage >&2
  exit 1
fi

APP_PATH="${PAYLOAD_DIR}/Platypus.app"
PLUGIN_PATH="${PAYLOAD_DIR}/Platypus.plugin"
USER_GUIDE_PATH="${PAYLOAD_DIR}/User Guide.pdf"
INSTALL_GUIDE_PATH="${PAYLOAD_DIR}/INSTALL Photoshop Plugin.txt"
PAYLOAD_BASENAME="$(basename "${PAYLOAD_DIR}")"

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

bundle_main_binary() {
  local bundle_path="$1"
  local bundle_name
  bundle_name="$(basename "${bundle_path}")"
  bundle_name="${bundle_name%.app}"
  bundle_name="${bundle_name%.plugin}"
  printf '%s/Contents/MacOS/%s\n' "${bundle_path}" "${bundle_name}"
}

check_payload_files() {
  [[ -d "${APP_PATH}" ]] &&
    [[ -d "${PLUGIN_PATH}" ]] &&
    [[ -f "${USER_GUIDE_PATH}" ]] &&
    [[ -f "${INSTALL_GUIDE_PATH}" ]]
}

check_bundle_signature_metadata() {
  local bundle_path="$1"
  local log_name="$2"
  local log_file="${REPORT_DIR}/${log_name}.log"
  local expected_executable_name
  local expected_executable_pattern

  expected_executable_name="$(basename "$(bundle_main_binary "${bundle_path}")")"
  expected_executable_pattern="^Executable=.*/Contents/MacOS/${expected_executable_name}$"

  codesign -dvv "${bundle_path}" >"${log_file}" 2>&1
  grep -q "Authority=Developer ID Application:" "${log_file}" &&
    grep -q "Runtime Version=" "${log_file}" &&
    grep -Eq "${expected_executable_pattern}" "${log_file}"
}

check_bundle_plist_metadata() {
  local bundle_path="$1"
  local log_name="$2"
  local log_file="${REPORT_DIR}/${log_name}.log"
  local info_plist="${bundle_path}/Contents/Info.plist"
  local expected_executable
  local actual_executable

  [[ -f "${info_plist}" ]] || {
    echo "Missing Info.plist: ${info_plist}" >"${log_file}"
    return 1
  }

  expected_executable="$(basename "$(bundle_main_binary "${bundle_path}")")"
  actual_executable="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "${info_plist}" 2>"${log_file}")" || {
    return 1
  }

  {
    echo "Info.plist: ${info_plist}"
    echo "Expected executable: ${expected_executable}"
    echo "Actual executable: ${actual_executable}"
  } >"${log_file}"

  [[ "${actual_executable}" == "${expected_executable}" ]]
}

check_nested_code_signatures() {
  local bundle_path="$1"
  local log_name="$2"
  local log_file="${REPORT_DIR}/${log_name}.log"
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
  done < <(find "${bundle_path}/Contents" -name '*.framework' -print0 | sort -z)

  while IFS= read -r -d '' file; do
    if [[ "${file}" == "${bundle_path}/Contents/MacOS/"* ]]; then
      continue
    fi
    if ! codesign --verify --strict --verbose=2 "${file}" >>"${log_file}" 2>&1; then
      echo "Failed code signature verification: ${file}" >>"${log_file}"
      return 1
    fi
  done < <(find "${bundle_path}/Contents" \
      \( -name '*.framework' -o -name '*.app' -o -name '*.bundle' -o -name '*.plugin' -o -name '*.xpc' \) -prune -o \
      -type f \( -name '*.dylib' -o -name '*.so' -o -perm -111 \) -print0 | sort -z)
}

check_release_entitlements() {
  local bundle_path="$1"
  local log_prefix="$2"
  local entitlements_file="${REPORT_DIR}/${log_prefix}-entitlements.xml"
  local stderr_file="${REPORT_DIR}/${log_prefix}-entitlements.stderr"

  codesign -d --entitlements - "${bundle_path}" >"${entitlements_file}" 2>"${stderr_file}" || true

  if grep -Eq "<plist|<dict|<key>" "${entitlements_file}"; then
    echo "Unexpected entitlements in ${bundle_path}" >&2
    cat "${entitlements_file}" >&2
    return 1
  fi
}

check_dependency_paths() {
  local bundle_path="$1"
  local log_name="$2"
  local log_file="${REPORT_DIR}/${log_name}.log"
  local binary_path
  local dep
  local binary
  local skip_self_id

  : >"${log_file}"
  binary_path="$(bundle_main_binary "${bundle_path}")"
  [[ -f "${binary_path}" ]] || {
    echo "Bundle executable not found: ${binary_path}" >&2
    return 1
  }

  while IFS= read -r -d '' binary; do
    echo "Dependencies for ${binary}" >>"${log_file}"
    otool -L "${binary}" >>"${log_file}" 2>&1
    if file -b "${binary}" | grep -Eq 'dynamically linked shared library|bundle'; then
      skip_self_id=1
    else
      skip_self_id=0
    fi
    while IFS= read -r dep; do
      [[ -n "${dep}" ]] || continue
      case "${dep}" in
        @executable_path/*|@loader_path/*|@rpath/*|/System/Library/*|/usr/lib/*)
          ;;
        /opt/homebrew/*|/usr/local/*|/Users/*|/private/*|*vcpkg*|*Cellar*)
          echo "Disallowed dependency path in ${binary}: ${dep}" >&2
          return 1
          ;;
      esac
    done < <(otool -L "${binary}" | awk -v skip_self_id="${skip_self_id}" '
      NR == 1 { next }
      skip_self_id && NR == 2 { next }
      { print $1 }
    ')
  done < <(find "${bundle_path}/Contents" \
      \( -name '*.framework' -o -name '*.app' -o -name '*.bundle' -o -name '*.plugin' -o -name '*.xpc' \) -prune -o \
      -type f \( -name '*.dylib' -o -name '*.so' -o -perm -111 \) -print0 | sort -z)
}

check_bundle_deployment_target() {
  local bundle_path="$1"
  local log_name="$2"
  local target_version="$3"
  local log_file="${REPORT_DIR}/${log_name}.log"
  local checked_count=0
  local binary_path=""
  local minos=""

  : >"${log_file}"

  while IFS= read -r -d '' binary_path; do
    checked_count=$((checked_count + 1))
    minos="$(extract_minos "${binary_path}")" || {
      echo "Unable to determine minimum macOS version for ${binary_path}" >>"${log_file}"
      return 1
    }
    printf '%s minos %s\n' "${binary_path}" "${minos}" >>"${log_file}"
    if version_gt "${minos}" "${target_version}"; then
      echo "Binary requires macOS ${minos}, above requested target ${target_version}: ${binary_path}" >>"${log_file}"
      return 1
    fi
  done < <(collect_bundle_macho_files "${bundle_path}")

  if [[ "${checked_count}" -eq 0 ]]; then
    echo "No Mach-O files found in ${bundle_path}" >>"${log_file}"
    return 1
  fi
}

check_dmg_contents() {
  local mount_dir
  mount_dir="$(mktemp -d "${TMPDIR:-/tmp}/platypus-photoshop-dmg.XXXXXX")"
  trap "hdiutil detach \"${mount_dir}\" >/dev/null 2>&1 || true; rm -rf \"${mount_dir}\"" RETURN

  hdiutil attach -readonly -nobrowse -mountpoint "${mount_dir}" "${DMG_PATH}" >/dev/null
  [[ -d "${mount_dir}/${PAYLOAD_BASENAME}" ]] &&
    [[ -d "${mount_dir}/${PAYLOAD_BASENAME}/Platypus.app" ]] &&
    [[ -d "${mount_dir}/${PAYLOAD_BASENAME}/Platypus.plugin" ]] &&
    [[ -f "${mount_dir}/${PAYLOAD_BASENAME}/User Guide.pdf" ]] &&
    [[ -f "${mount_dir}/${PAYLOAD_BASENAME}/INSTALL Photoshop Plugin.txt" ]]
}

run_check payload-files check_payload_files
run_check app-plist-metadata check_bundle_plist_metadata "${APP_PATH}" app-plist-metadata
run_check app-signature check_bundle_signature_metadata "${APP_PATH}" app-codesign-display
run_check app-nested-codesign check_nested_code_signatures "${APP_PATH}" app-nested-codesign
run_check app-entitlements check_release_entitlements "${APP_PATH}" app
run_check app-dependency-paths check_dependency_paths "${APP_PATH}" app-otool-main-executable
run_check plugin-plist-metadata check_bundle_plist_metadata "${PLUGIN_PATH}" plugin-plist-metadata
run_check plugin-signature check_bundle_signature_metadata "${PLUGIN_PATH}" plugin-codesign-display
run_check plugin-nested-codesign check_nested_code_signatures "${PLUGIN_PATH}" plugin-nested-codesign
run_check plugin-entitlements check_release_entitlements "${PLUGIN_PATH}" plugin
run_check plugin-dependency-paths check_dependency_paths "${PLUGIN_PATH}" plugin-otool-main-executable
if [[ -n "${DEPLOYMENT_TARGET}" ]]; then
  run_check app-deployment-target check_bundle_deployment_target "${APP_PATH}" app-deployment-target "${DEPLOYMENT_TARGET}"
  run_check plugin-deployment-target check_bundle_deployment_target "${PLUGIN_PATH}" plugin-deployment-target "${DEPLOYMENT_TARGET}"
fi

if [[ "${PHASE}" == "post-staple" ]]; then
  if [[ ! -f "${DMG_PATH}" ]]; then
    echo "DMG not found: ${DMG_PATH}" >&2
    exit 1
  fi
  run_check dmg-codesign codesign --verify --verbose=2 "${DMG_PATH}"
  run_check dmg-stapler xcrun stapler validate "${DMG_PATH}"
  run_check dmg-spctl spctl -a -t open --context context:primary-signature -v "${DMG_PATH}"
  run_check dmg-contents check_dmg_contents
fi

if [[ "${FAILURES}" -ne 0 ]]; then
  echo "Photoshop macOS release verification failed (${FAILURES} check(s))" >&2
  exit 1
fi

echo "All Photoshop macOS release verification checks passed."
