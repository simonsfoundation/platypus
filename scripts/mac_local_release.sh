#!/bin/bash

set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  scripts/mac_local_release.sh [options]

Options:
  --profile <standard|macos13>   Release profile to build. Default: standard
  --package-version <version>    Override package version embedded in artifacts
  --build-root <path>            Root output directory. Default: build-local/<profile>/<host-arch>
  --skip-deps                    Skip dependency installation/bootstrap
  -h, --help                     Show this help text

Environment:
  If a .env file exists in the repo root, it is loaded automatically.

  Required for signing/notarization:
    APPLE_SIGNING_IDENTITY
    APPSTORE_CONNECT_API_KEY_PATH
    APPSTORE_CONNECT_API_KEY_ID
    APPSTORE_CONNECT_API_ISSUER_ID

  Optional local signing helpers:
    MACOS_CERT_P12_PATH
    MACOS_CERT_P12_PASSWORD

Examples:
  scripts/mac_local_release.sh
  scripts/mac_local_release.sh --profile macos13
  scripts/mac_local_release.sh --profile standard --package-version 0.3.3-local
EOF
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
ENV_FILE="${REPO_ROOT}/.env"

load_repo_env() {
  local env_file="$1"
  if [[ ! -f "${env_file}" ]]; then
    return
  fi

  while IFS= read -r line || [[ -n "${line}" ]]; do
    [[ -n "${line}" ]] || continue
    [[ "${line}" =~ ^[[:space:]]*# ]] && continue
    if [[ "${line}" == *=* ]]; then
      local key="${line%%=*}"
      local value="${line#*=}"
      key="${key#"${key%%[![:space:]]*}"}"
      key="${key%"${key##*[![:space:]]}"}"
      export "${key}=${value}"
    fi
  done < "${env_file}"
}

log_step() {
  printf '\n==> %s\n' "$1"
}

fail() {
  echo "$1" >&2
  exit 1
}

trim_whitespace() {
  local value="$1"
  value="${value#"${value%%[![:space:]]*}"}"
  value="${value%"${value##*[![:space:]]}"}"
  printf '%s' "${value}"
}

require_file() {
  local path="$1"
  [[ -f "${path}" ]] || fail "Required file not found: ${path}"
}

require_dir() {
  local path="$1"
  [[ -d "${path}" ]] || fail "Required directory not found: ${path}"
}

require_command() {
  command -v "$1" >/dev/null 2>&1 || fail "Required command not found: $1"
}

install_brew_packages() {
  local package_file="$1"
  local packages

  packages="$(grep -Ev '^[[:space:]]*(#|$)' "${package_file}" | tr '\n' ' ')"
  [[ -n "${packages}" ]] || return

  log_step "Installing Homebrew dependencies from $(basename "${package_file}")"
  HOMEBREW_NO_AUTO_UPDATE=1 HOMEBREW_NO_INSTALL_CLEANUP=1 brew install ${packages}
}

derive_package_version() {
  local derived_version=""

  if git -C "${REPO_ROOT}" describe --tags --exact-match >/dev/null 2>&1; then
    derived_version="$(git -C "${REPO_ROOT}" describe --tags --exact-match)"
    derived_version="${derived_version#v}"
  else
    derived_version="0.0.0-local-$(git -C "${REPO_ROOT}" rev-parse --short HEAD 2>/dev/null || echo unknown)"
  fi

  printf '%s\n' "${derived_version}"
}

verify_binary_arch() {
  local binary_path="$1"
  local expected_arch="$2"
  local archs

  archs="$(lipo -archs "${binary_path}")"
  echo "Architectures for ${binary_path}: ${archs}"
  [[ "${archs}" == *"${expected_arch}"* ]] || fail "Missing expected architecture ${expected_arch} in ${binary_path}"
}

prepare_keychain() {
  local cert_path="${MACOS_CERT_P12_PATH:-${REPO_ROOT}/../dev_app.p12}"
  local cert_password="${MACOS_CERT_P12_PASSWORD:-}"
  local line=""

  require_file "${cert_path}"

  ORIGINAL_DEFAULT_KEYCHAIN="$(trim_whitespace "$(security default-keychain -d user | tr -d '"')")"
  ORIGINAL_KEYCHAINS=()
  while IFS= read -r line; do
    line="$(trim_whitespace "${line//\"/}")"
    [[ -n "${line}" ]] && ORIGINAL_KEYCHAINS+=("${line}")
  done < <(security list-keychains -d user)

  KEYCHAIN_PATH="${TMPDIR:-/tmp}/platypus-local-release.$(uuidgen).keychain-db"
  KEYCHAIN_PASSWORD="$(uuidgen)"

  log_step "Importing signing certificate into a temporary keychain"
  security create-keychain -p "${KEYCHAIN_PASSWORD}" "${KEYCHAIN_PATH}"
  security set-keychain-settings -lut 21600 "${KEYCHAIN_PATH}"
  security unlock-keychain -p "${KEYCHAIN_PASSWORD}" "${KEYCHAIN_PATH}"
  security import "${cert_path}" -k "${KEYCHAIN_PATH}" -P "${cert_password}" -T /usr/bin/codesign -T /usr/bin/security

  if [[ "${#ORIGINAL_KEYCHAINS[@]}" -gt 0 ]]; then
    security list-keychains -d user -s "${KEYCHAIN_PATH}" "${ORIGINAL_KEYCHAINS[@]}"
  else
    security list-keychains -d user -s "${KEYCHAIN_PATH}"
  fi
  security default-keychain -d user -s "${KEYCHAIN_PATH}"
  security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "${KEYCHAIN_PASSWORD}" "${KEYCHAIN_PATH}"

  security find-identity -v -p codesigning "${KEYCHAIN_PATH}" | tee "${VERIFY_ROOT}/codesigning-identities.log"
  grep -F "${APPLE_SIGNING_IDENTITY}" "${VERIFY_ROOT}/codesigning-identities.log" >/dev/null 2>&1 || \
    fail "Imported keychain does not contain signing identity: ${APPLE_SIGNING_IDENTITY}"
}

cleanup_keychain() {
  if [[ -n "${ORIGINAL_DEFAULT_KEYCHAIN}" ]]; then
    security default-keychain -d user -s "${ORIGINAL_DEFAULT_KEYCHAIN}" >/dev/null 2>&1 || true
  fi

  if [[ "${#ORIGINAL_KEYCHAINS[@]}" -gt 0 ]]; then
    security list-keychains -d user -s "${ORIGINAL_KEYCHAINS[@]}" >/dev/null 2>&1 || true
  fi

  if [[ -n "${KEYCHAIN_PATH}" && -f "${KEYCHAIN_PATH}" ]]; then
    security delete-keychain "${KEYCHAIN_PATH}" >/dev/null 2>&1 || rm -f "${KEYCHAIN_PATH}"
  fi
}

bootstrap_vcpkg() {
  require_dir "${REPO_ROOT}/vcpkg"
  if [[ ! -x "${REPO_ROOT}/vcpkg/vcpkg" ]]; then
    log_step "Bootstrapping vcpkg"
    "${REPO_ROOT}/vcpkg/bootstrap-vcpkg.sh" -disableMetrics
  fi
}

configure_standard_builds() {
  local qt_prefix
  local opencv_prefix

  qt_prefix="$(brew --prefix qt)"
  opencv_prefix="$(brew --prefix opencv)"

  log_step "Configuring standard app build"
  cmake -B "${APP_BUILD_DIR}" -S "${REPO_ROOT}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_APPLE_APP=ON \
    -DBUILD_TESTING=OFF \
    -DCMAKE_OSX_ARCHITECTURES="${HOST_ARCH}" \
    -DPLATYPUS_PACKAGE_VERSION_OVERRIDE="${PACKAGE_VERSION}" \
    -DCMAKE_PREFIX_PATH="${qt_prefix};${opencv_prefix}" \
    -DOpenCV_DIR="${opencv_prefix}/lib/cmake/opencv4"

  log_step "Configuring standard Photoshop build"
  cmake -B "${PHOTOSHOP_BUILD_DIR}" -S "${REPO_ROOT}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_PHOTOSHOP_PLUGIN=ON \
    -DBUILD_TESTING=OFF \
    -DCMAKE_OSX_ARCHITECTURES="${HOST_ARCH}" \
    -DPLATYPUS_PACKAGE_VERSION_OVERRIDE="${PACKAGE_VERSION}" \
    -DCMAKE_PREFIX_PATH="${qt_prefix};${opencv_prefix}" \
    -DOpenCV_DIR="${opencv_prefix}/lib/cmake/opencv4"
}

configure_macos13_builds() {
  local qt_tools_dir=""
  local qt_paths=""
  local macdeployqt=""

  export MACOSX_DEPLOYMENT_TARGET="13.0"
  export PLATYPUS_MACOS_DEPLOYMENT_TARGET="13.0"
  export VCPKG_DISABLE_METRICS=1
  export VCPKG_DEFAULT_BINARY_CACHE="${VCPKG_BINARY_CACHE_DIR}"
  export VCPKG_BINARY_SOURCES="clear;files,${VCPKG_BINARY_CACHE_DIR},readwrite"

  bootstrap_vcpkg

  log_step "Installing macOS 13 vcpkg dependencies"
  "${REPO_ROOT}/vcpkg/vcpkg" install \
    --clean-buildtrees-after-build \
    --x-manifest-root="${REPO_ROOT}/.github/vcpkg-manifests/macos13" \
    --x-install-root="${VCPKG_INSTALLED_DIR}" \
    --overlay-triplets="${REPO_ROOT}/.github/vcpkg-triplets" \
    --triplet="${VCPKG_TARGET_TRIPLET}" \
    --host-triplet="${VCPKG_HOST_TRIPLET}"

  qt_tools_dir="${VCPKG_INSTALLED_DIR}/${VCPKG_HOST_TRIPLET}/tools/Qt6/bin"
  if [[ -x "${qt_tools_dir}/qtpaths6" ]]; then
    qt_paths="${qt_tools_dir}/qtpaths6"
  else
    qt_paths="${qt_tools_dir}/qtpaths"
  fi

  if [[ -x "${qt_tools_dir}/macdeployqt6" ]]; then
    macdeployqt="${qt_tools_dir}/macdeployqt6"
  else
    macdeployqt="${qt_tools_dir}/macdeployqt"
  fi

  require_file "${qt_paths}"
  require_file "${macdeployqt}"

  log_step "Configuring macOS 13 app build"
  cmake -B "${APP_BUILD_DIR}" -S "${REPO_ROOT}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_APPLE_APP=ON \
    -DBUILD_TESTING=OFF \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="${PLATYPUS_MACOS_DEPLOYMENT_TARGET}" \
    -DCMAKE_OSX_ARCHITECTURES="${HOST_ARCH}" \
    -DPLATYPUS_PACKAGE_VERSION_OVERRIDE="${PACKAGE_VERSION}" \
    -DCMAKE_TOOLCHAIN_FILE="${REPO_ROOT}/vcpkg/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_MANIFEST_MODE=OFF \
    -DVCPKG_OVERLAY_TRIPLETS="${REPO_ROOT}/.github/vcpkg-triplets" \
    -DVCPKG_INSTALLED_DIR="${VCPKG_INSTALLED_DIR}" \
    -DVCPKG_TARGET_TRIPLET="${VCPKG_TARGET_TRIPLET}" \
    -DVCPKG_HOST_TRIPLET="${VCPKG_HOST_TRIPLET}" \
    -DCMAKE_PREFIX_PATH="${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}" \
    -DQt6_DIR="${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/Qt6" \
    -DOpenCV_DIR="${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/opencv4" \
    -DMACDEPLOYQT_EXECUTABLE="${macdeployqt}" \
    -DQT_PATHS_EXECUTABLE="${qt_paths}"

  log_step "Configuring macOS 13 Photoshop build"
  cmake -B "${PHOTOSHOP_BUILD_DIR}" -S "${REPO_ROOT}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_PHOTOSHOP_PLUGIN=ON \
    -DBUILD_TESTING=OFF \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="${PLATYPUS_MACOS_DEPLOYMENT_TARGET}" \
    -DCMAKE_OSX_ARCHITECTURES="${HOST_ARCH}" \
    -DPLATYPUS_PACKAGE_VERSION_OVERRIDE="${PACKAGE_VERSION}" \
    -DCMAKE_TOOLCHAIN_FILE="${REPO_ROOT}/vcpkg/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_MANIFEST_MODE=OFF \
    -DVCPKG_OVERLAY_TRIPLETS="${REPO_ROOT}/.github/vcpkg-triplets" \
    -DVCPKG_INSTALLED_DIR="${VCPKG_INSTALLED_DIR}" \
    -DVCPKG_TARGET_TRIPLET="${VCPKG_TARGET_TRIPLET}" \
    -DVCPKG_HOST_TRIPLET="${VCPKG_HOST_TRIPLET}" \
    -DCMAKE_PREFIX_PATH="${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}" \
    -DQt6_DIR="${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/Qt6" \
    -DOpenCV_DIR="${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/opencv4" \
    -DMACDEPLOYQT_EXECUTABLE="${macdeployqt}" \
    -DQT_PATHS_EXECUTABLE="${qt_paths}"
}

run_release_verification() {
  local app_phase="$1"
  local ps_phase="$2"

  if [[ "${PROFILE}" == "macos13" ]]; then
    bash "${SCRIPT_DIR}/mac_verify_release.sh" \
      --phase "${app_phase}" \
      --app "${APP_PATH}" \
      --dmg "${APP_DMG_PATH}" \
      --deployment-target "${PLATYPUS_MACOS_DEPLOYMENT_TARGET}" \
      --report-dir "${VERIFY_APP_DIR}/${app_phase}"

    bash "${SCRIPT_DIR}/mac_verify_photoshop_release.sh" \
      --phase "${ps_phase}" \
      --payload-dir "${PHOTOSHOP_PAYLOAD_DIR}" \
      --dmg "${PHOTOSHOP_DMG_PATH}" \
      --deployment-target "${PLATYPUS_MACOS_DEPLOYMENT_TARGET}" \
      --report-dir "${VERIFY_PHOTOSHOP_DIR}/${ps_phase}"
  else
    bash "${SCRIPT_DIR}/mac_verify_release.sh" \
      --phase "${app_phase}" \
      --app "${APP_PATH}" \
      --dmg "${APP_DMG_PATH}" \
      --report-dir "${VERIFY_APP_DIR}/${app_phase}"

    bash "${SCRIPT_DIR}/mac_verify_photoshop_release.sh" \
      --phase "${ps_phase}" \
      --payload-dir "${PHOTOSHOP_PAYLOAD_DIR}" \
      --dmg "${PHOTOSHOP_DMG_PATH}" \
      --report-dir "${VERIFY_PHOTOSHOP_DIR}/${ps_phase}"
  fi
}

PROFILE="standard"
PACKAGE_VERSION=""
BUILD_ROOT=""
SKIP_DEPS=0
HOST_ARCH="$(uname -m)"
ORIGINAL_DEFAULT_KEYCHAIN=""
ORIGINAL_KEYCHAINS=()
KEYCHAIN_PATH=""
KEYCHAIN_PASSWORD=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --profile)
      [[ $# -ge 2 ]] || fail "Missing value for $1"
      PROFILE="$2"
      shift 2
      ;;
    --package-version)
      [[ $# -ge 2 ]] || fail "Missing value for $1"
      PACKAGE_VERSION="$2"
      shift 2
      ;;
    --build-root)
      [[ $# -ge 2 ]] || fail "Missing value for $1"
      BUILD_ROOT="$2"
      shift 2
      ;;
    --skip-deps)
      SKIP_DEPS=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      fail "Unknown argument: $1"
      ;;
  esac
done

load_repo_env "${ENV_FILE}"

case "${PROFILE}" in
  standard|macos13)
    ;;
  *)
    fail "Unsupported profile: ${PROFILE}"
    ;;
esac

case "${HOST_ARCH}" in
  arm64|x86_64)
    ;;
  *)
    fail "Unsupported host architecture: ${HOST_ARCH}"
    ;;
esac

require_command cmake
require_command security
require_command codesign
require_command xcrun
require_command hdiutil
require_command shasum
require_command brew
require_command git

APPLE_SIGNING_IDENTITY="${APPLE_SIGNING_IDENTITY:-}"
APPSTORE_CONNECT_API_KEY_PATH="${APPSTORE_CONNECT_API_KEY_PATH:-}"
APPSTORE_CONNECT_API_KEY_ID="${APPSTORE_CONNECT_API_KEY_ID:-}"
APPSTORE_CONNECT_API_ISSUER_ID="${APPSTORE_CONNECT_API_ISSUER_ID:-}"

[[ -n "${APPLE_SIGNING_IDENTITY}" ]] || fail "APPLE_SIGNING_IDENTITY is required"
[[ -n "${APPSTORE_CONNECT_API_KEY_PATH}" ]] || fail "APPSTORE_CONNECT_API_KEY_PATH is required"
[[ -n "${APPSTORE_CONNECT_API_KEY_ID}" ]] || fail "APPSTORE_CONNECT_API_KEY_ID is required"
[[ -n "${APPSTORE_CONNECT_API_ISSUER_ID}" ]] || fail "APPSTORE_CONNECT_API_ISSUER_ID is required"
require_file "${APPSTORE_CONNECT_API_KEY_PATH}"

if [[ -z "${PACKAGE_VERSION}" ]]; then
  PACKAGE_VERSION="$(derive_package_version)"
fi

if [[ -z "${BUILD_ROOT}" ]]; then
  BUILD_ROOT="${REPO_ROOT}/build-local/${PROFILE}/${HOST_ARCH}"
fi

APP_BUILD_DIR="${BUILD_ROOT}/app-build"
PHOTOSHOP_BUILD_DIR="${BUILD_ROOT}/photoshop-build"
APP_STAGE_DIR="${BUILD_ROOT}/stage/app"
RELEASE_DIR="${BUILD_ROOT}/release"
VERIFY_ROOT="${BUILD_ROOT}/verify"
VERIFY_APP_DIR="${VERIFY_ROOT}/app"
VERIFY_PHOTOSHOP_DIR="${VERIFY_ROOT}/photoshop"
VCPKG_INSTALLED_DIR="${BUILD_ROOT}/vcpkg_installed"
VCPKG_BINARY_CACHE_DIR="${BUILD_ROOT}/vcpkg-bincache"

if [[ "${PROFILE}" == "macos13" ]]; then
  PROFILE_LABEL="macOS 13"
  VCPKG_TARGET_TRIPLET=$([[ "${HOST_ARCH}" == "arm64" ]] && echo "arm64-osx-dynamic-13-release" || echo "x64-osx-dynamic-13-release")
  VCPKG_HOST_TRIPLET="${VCPKG_TARGET_TRIPLET}"
  APP_DMG_NAME="Platypus-macos13-${HOST_ARCH}.dmg"
  APP_CHECKSUM_NAME="Platypus-macos13-${HOST_ARCH}.sha256"
  PHOTOSHOP_DMG_NAME="Platypus-photoshop-macos13-${HOST_ARCH}.dmg"
  PHOTOSHOP_CHECKSUM_NAME="Platypus-photoshop-macos13-${HOST_ARCH}.sha256"
else
  PROFILE_LABEL="standard"
  APP_DMG_NAME="Platypus-macos-${HOST_ARCH}.dmg"
  APP_CHECKSUM_NAME="Platypus-macos-${HOST_ARCH}.sha256"
  PHOTOSHOP_DMG_NAME="Platypus-photoshop-macos-${HOST_ARCH}.dmg"
  PHOTOSHOP_CHECKSUM_NAME="Platypus-photoshop-macos-${HOST_ARCH}.sha256"
fi

APP_DMG_PATH="${RELEASE_DIR}/${APP_DMG_NAME}"
APP_CHECKSUM_PATH="${RELEASE_DIR}/${APP_CHECKSUM_NAME}"
PHOTOSHOP_DMG_PATH="${RELEASE_DIR}/${PHOTOSHOP_DMG_NAME}"
PHOTOSHOP_CHECKSUM_PATH="${RELEASE_DIR}/${PHOTOSHOP_CHECKSUM_NAME}"

mkdir -p "${BUILD_ROOT}" "${VCPKG_BINARY_CACHE_DIR}"
rm -rf "${APP_BUILD_DIR}" "${PHOTOSHOP_BUILD_DIR}" "${APP_STAGE_DIR}" "${RELEASE_DIR}" "${VERIFY_ROOT}"
mkdir -p "${RELEASE_DIR}" "${VERIFY_APP_DIR}" "${VERIFY_PHOTOSHOP_DIR}"

if [[ "${SKIP_DEPS}" -eq 0 ]]; then
  if [[ "${PROFILE}" == "macos13" ]]; then
    install_brew_packages "${REPO_ROOT}/.github/homebrew/release-macos13.txt"
  else
    install_brew_packages "${REPO_ROOT}/.github/homebrew/release-macos.txt"
  fi
fi

if [[ "${PROFILE}" == "macos13" ]]; then
  configure_macos13_builds
else
  configure_standard_builds
fi

log_step "Building PlatypusGui (${PROFILE_LABEL}, ${HOST_ARCH})"
cmake --build "${APP_BUILD_DIR}" --config Release --target PlatypusGui
verify_binary_arch "${APP_BUILD_DIR}/PlatypusGui.app/Contents/MacOS/PlatypusGui" "${HOST_ARCH}"

log_step "Staging app bundle"
cmake --install "${APP_BUILD_DIR}" --prefix "${APP_STAGE_DIR}"
APP_PATH="${APP_STAGE_DIR}/PlatypusGui.app"
require_dir "${APP_PATH}"

log_step "Building Photoshop release payload"
cmake --build "${PHOTOSHOP_BUILD_DIR}" --config Release --target stage_photoshop_release
PHOTOSHOP_PAYLOAD_DIR="${PHOTOSHOP_BUILD_DIR}/photoshop-release/photoshop/Platypus v${PACKAGE_VERSION}"
PHOTOSHOP_APP_PATH="${PHOTOSHOP_PAYLOAD_DIR}/Platypus.app"
PHOTOSHOP_PLUGIN_PATH="${PHOTOSHOP_PAYLOAD_DIR}/Platypus.plugin"
require_dir "${PHOTOSHOP_PAYLOAD_DIR}"
require_dir "${PHOTOSHOP_APP_PATH}"
require_dir "${PHOTOSHOP_PLUGIN_PATH}"
verify_binary_arch "${PHOTOSHOP_APP_PATH}/Contents/MacOS/Platypus" "${HOST_ARCH}"
verify_binary_arch "${PHOTOSHOP_PLUGIN_PATH}/Contents/MacOS/Platypus" "${HOST_ARCH}"

trap cleanup_keychain EXIT
prepare_keychain

log_step "Signing app bundle"
bash "${SCRIPT_DIR}/mac_sign_app.sh" \
  --app "${APP_PATH}" \
  --identity "${APPLE_SIGNING_IDENTITY}" \
  --skip-deploy

log_step "Signing Photoshop companion app"
bash "${SCRIPT_DIR}/mac_sign_bundle.sh" \
  --bundle "${PHOTOSHOP_APP_PATH}" \
  --identity "${APPLE_SIGNING_IDENTITY}"

log_step "Signing Photoshop plugin bundle"
bash "${SCRIPT_DIR}/mac_sign_bundle.sh" \
  --bundle "${PHOTOSHOP_PLUGIN_PATH}" \
  --identity "${APPLE_SIGNING_IDENTITY}"

log_step "Running pre-notarize verification"
run_release_verification "pre-notarize" "pre-notarize"

log_step "Creating signed app DMG"
bash "${SCRIPT_DIR}/mac_package_dmg.sh" \
  --app "${APP_PATH}" \
  --dmg "${APP_DMG_PATH}" \
  --identity "${APPLE_SIGNING_IDENTITY}" \
  --volname "Platypus"

log_step "Creating signed Photoshop DMG"
bash "${SCRIPT_DIR}/mac_package_dir_dmg.sh" \
  --source-dir "${PHOTOSHOP_PAYLOAD_DIR}" \
  --dmg "${PHOTOSHOP_DMG_PATH}" \
  --identity "${APPLE_SIGNING_IDENTITY}" \
  --volname "Platypus Photoshop"

log_step "Notarizing app DMG"
bash "${SCRIPT_DIR}/mac_notarize_dmg.sh" \
  --dmg "${APP_DMG_PATH}" \
  --api-key "${APPSTORE_CONNECT_API_KEY_PATH}" \
  --key-id "${APPSTORE_CONNECT_API_KEY_ID}" \
  --issuer "${APPSTORE_CONNECT_API_ISSUER_ID}" \
  --skip-dmg

log_step "Notarizing Photoshop DMG"
bash "${SCRIPT_DIR}/mac_notarize_dmg.sh" \
  --dmg "${PHOTOSHOP_DMG_PATH}" \
  --api-key "${APPSTORE_CONNECT_API_KEY_PATH}" \
  --key-id "${APPSTORE_CONNECT_API_KEY_ID}" \
  --issuer "${APPSTORE_CONNECT_API_ISSUER_ID}" \
  --skip-dmg

log_step "Running post-staple verification"
run_release_verification "post-staple" "post-staple"

log_step "Generating checksums"
shasum -a 256 "${APP_DMG_PATH}" > "${APP_CHECKSUM_PATH}"
shasum -a 256 "${PHOTOSHOP_DMG_PATH}" > "${PHOTOSHOP_CHECKSUM_PATH}"

cat <<EOF

Local macOS release complete.
Profile: ${PROFILE}
Architecture: ${HOST_ARCH}
Package version: ${PACKAGE_VERSION}

App bundle:
  ${APP_PATH}
App DMG:
  ${APP_DMG_PATH}
Photoshop payload:
  ${PHOTOSHOP_PAYLOAD_DIR}
Photoshop DMG:
  ${PHOTOSHOP_DMG_PATH}
Verification:
  ${VERIFY_ROOT}
EOF
