#!/usr/bin/env bash
set -euo pipefail

# Platypus build configuration script
# Prefers preinstalled Qt6/OpenCV when available and falls back to vcpkg otherwise
#
# Options:
#   --update-vcpkg    Pull latest vcpkg and re-bootstrap (forces vcpkg)
#   --clean-cache     Clear the vcpkg binary cache
#   All other args are forwarded to cmake

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VCPKG_DIR="${SCRIPT_DIR}/vcpkg"
BUILD_DIR="${SCRIPT_DIR}/build"

# Persistent binary cache — survives rm -rf build
VCPKG_CACHE_DIR="${HOME}/.cache/vcpkg/archives"

# ---------- Color helpers ----------
red()   { printf '\033[1;31m%s\033[0m\n' "$*"; }
green() { printf '\033[1;32m%s\033[0m\n' "$*"; }
yellow(){ printf '\033[1;33m%s\033[0m\n' "$*"; }

append_prefix_path() {
    local prefix="${1:-}"
    [[ -n "${prefix}" ]] || return 0
    SYSTEM_PREFIXES+=("${prefix}")
}

join_by_semicolon() {
    local result=""
    local item
    for item in "$@"; do
        [[ -n "${item}" ]] || continue
        if [[ -z "${result}" ]]; then
            result="${item}"
        else
            result="${result};${item}"
        fi
    done
    printf '%s' "${result}"
}

# ---------- Parse our flags (strip them before forwarding to cmake) ----------
UPDATE_VCPKG=false
CLEAN_CACHE=false
CMAKE_EXTRA_ARGS=()
for arg in "$@"; do
    case "${arg}" in
        --update-vcpkg) UPDATE_VCPKG=true ;;
        --clean-cache)  CLEAN_CACHE=true ;;
        *)              CMAKE_EXTRA_ARGS+=("${arg}") ;;
    esac
done

# ---------- Detect platform ----------
OS="$(uname -s)"
case "${OS}" in
    Darwin*) PLATFORM="mac";;
    Linux*)  PLATFORM="linux";;
    *)       red "Unsupported platform: ${OS}"; exit 1;;
esac

echo "Detected platform: ${PLATFORM}"

# ---------- Check basic prerequisites ----------
for cmd in cmake; do
    if ! command -v "${cmd}" &>/dev/null; then
        red "Error: '${cmd}' is not installed."
        if [[ "${PLATFORM}" == "mac" ]]; then
            echo "  Install with: brew install ${cmd}"
        else
            echo "  Install with: sudo apt-get install -y ${cmd}"
        fi
        exit 1
    fi
done

# ---------- Check for Ninja ----------
if ! command -v ninja &>/dev/null; then
    if [[ "${PLATFORM}" == "mac" ]]; then
        yellow "Ninja not found. Installing via Homebrew..."
        if command -v brew &>/dev/null; then
            brew install ninja
        else
            red "Error: Ninja is required but Homebrew is not installed."
            echo "  Install Homebrew from https://brew.sh then run: brew install ninja"
            exit 1
        fi
    else
        red "Error: 'ninja' is not installed."
        echo "  Install with: sudo apt-get install -y ninja-build"
        exit 1
    fi
fi

ARCH=""
if [[ "${PLATFORM}" == "mac" ]]; then
    ARCH="$(uname -m)"
fi

SYSTEM_PREFIXES=()
SYSTEM_CMAKE_ARGS=()
SYSTEM_QT_VERSION=""
SYSTEM_OPENCV_VERSION=""

if command -v pkg-config &>/dev/null; then
    if pkg-config --exists Qt6Core; then
        SYSTEM_QT_VERSION="$(pkg-config --modversion Qt6Core 2>/dev/null || true)"
        append_prefix_path "$(pkg-config --variable=prefix Qt6Core 2>/dev/null || true)"
    fi
    if pkg-config --exists opencv4; then
        SYSTEM_OPENCV_VERSION="$(pkg-config --modversion opencv4 2>/dev/null || true)"
        append_prefix_path "$(pkg-config --variable=prefix opencv4 2>/dev/null || true)"
    fi
fi

if [[ "${PLATFORM}" == "mac" ]] && command -v brew &>/dev/null; then
    append_prefix_path "$(brew --prefix qt 2>/dev/null || true)"
    append_prefix_path "$(brew --prefix opencv 2>/dev/null || true)"
fi

SYSTEM_PREFIX_PATH="$(join_by_semicolon "${SYSTEM_PREFIXES[@]}")"
if [[ -n "${SYSTEM_PREFIX_PATH}" ]]; then
    SYSTEM_CMAKE_ARGS+=("-DCMAKE_PREFIX_PATH=${SYSTEM_PREFIX_PATH}")
fi

probe_system_deps() {
    local probe_dir
    local probe_args
    probe_dir="$(mktemp -d "${TMPDIR:-/tmp}/platypus-system-probe.XXXXXX")"
    probe_args=(
        -S "${SCRIPT_DIR}"
        -B "${probe_dir}"
        -G Ninja
        -DCMAKE_BUILD_TYPE=Release
    )

    if [[ "${PLATFORM}" == "mac" ]]; then
        probe_args+=(-DCMAKE_OSX_ARCHITECTURES="${ARCH}")
    fi

    probe_args+=("${SYSTEM_CMAKE_ARGS[@]}")

    if cmake "${probe_args[@]}" >/dev/null 2>&1; then
        rm -rf "${probe_dir}"
        return 0
    fi

    rm -rf "${probe_dir}"
    return 1
}

USE_SYSTEM_DEPS=false
if ! ${UPDATE_VCPKG} && probe_system_deps; then
    USE_SYSTEM_DEPS=true
    green "Using preinstalled Qt6/OpenCV."
    [[ -n "${SYSTEM_QT_VERSION}" ]] && echo "  Qt6: ${SYSTEM_QT_VERSION}"
    [[ -n "${SYSTEM_OPENCV_VERSION}" ]] && echo "  OpenCV: ${SYSTEM_OPENCV_VERSION}"
elif ${UPDATE_VCPKG}; then
    yellow "--update-vcpkg requested; forcing vcpkg toolchain."
else
    yellow "Preinstalled Qt6/OpenCV not detected by CMake. Falling back to vcpkg."
fi

if ! ${USE_SYSTEM_DEPS}; then
    if ! command -v git &>/dev/null; then
        red "Error: 'git' is not installed."
        if [[ "${PLATFORM}" == "mac" ]]; then
            echo "  Install with: brew install git"
        else
            echo "  Install with: sudo apt-get install -y git"
        fi
        exit 1
    fi

    if [[ "${PLATFORM}" == "linux" ]]; then
        echo "Checking Linux system packages required for vcpkg..."
        MISSING_PKGS=()

        REQUIRED_PKGS=(
            "pkg-config:pkg-config"
            "libx11-dev:libx11-dev"
            "libxext-dev:libxext-dev"
            "libxrender-dev:libxrender-dev"
            "libxcb1-dev:libxcb1-dev"
            "libx11-xcb-dev:libx11-xcb-dev"
            "libxcb-glx0-dev:libxcb-glx0-dev"
            "libxkbcommon-dev:libxkbcommon-dev"
            "libxkbcommon-x11-dev:libxkbcommon-x11-dev"
            "libgl1-mesa-dev:libgl1-mesa-dev"
            "linux-libc-dev:linux-libc-dev"
            "libxcb-cursor-dev:libxcb-cursor-dev"
            "libxcb-icccm4-dev:libxcb-icccm4-dev"
            "libxcb-image0-dev:libxcb-image0-dev"
            "libxcb-keysyms1-dev:libxcb-keysyms1-dev"
            "libxcb-randr0-dev:libxcb-randr0-dev"
            "libxcb-render-util0-dev:libxcb-render-util0-dev"
            "libxcb-shape0-dev:libxcb-shape0-dev"
            "libxcb-shm0-dev:libxcb-shm0-dev"
            "libxcb-sync-dev:libxcb-sync-dev"
            "libxcb-xfixes0-dev:libxcb-xfixes0-dev"
            "libxcb-xinerama0-dev:libxcb-xinerama0-dev"
            "libxcb-xkb-dev:libxcb-xkb-dev"
            "curl:curl"
            "zip:zip"
            "unzip:unzip"
            "tar:tar"
            "ninja-build:ninja-build"
        )

        for entry in "${REQUIRED_PKGS[@]}"; do
            pkg="${entry%%:*}"
            if ! dpkg -s "${pkg}" &>/dev/null 2>&1; then
                MISSING_PKGS+=("${pkg}")
            fi
        done

        if [[ ${#MISSING_PKGS[@]} -gt 0 ]]; then
            red "Missing required system packages:"
            echo "  ${MISSING_PKGS[*]}"
            echo ""
            yellow "Install them with:"
            echo "  sudo apt-get install -y ${MISSING_PKGS[*]}"
            exit 1
        fi
        green "All required system packages found."
    fi

    if ${CLEAN_CACHE}; then
        yellow "Clearing vcpkg binary cache at ${VCPKG_CACHE_DIR}..."
        rm -rf "${VCPKG_CACHE_DIR}"
    fi
    mkdir -p "${VCPKG_CACHE_DIR}"
    export VCPKG_DEFAULT_BINARY_CACHE="${VCPKG_CACHE_DIR}"
    green "Binary cache: ${VCPKG_CACHE_DIR}"

    if [[ ! -d "${VCPKG_DIR}" ]]; then
        echo "Cloning vcpkg..."
        git clone https://github.com/microsoft/vcpkg.git "${VCPKG_DIR}"
    elif ${UPDATE_VCPKG}; then
        echo "Updating vcpkg (--update-vcpkg)..."
        git -C "${VCPKG_DIR}" pull --ff-only || yellow "Warning: could not update vcpkg (not on a branch?)"
    else
        echo "Using existing vcpkg (pass --update-vcpkg to refresh)."
    fi

    if [[ ! -x "${VCPKG_DIR}/vcpkg" ]] || ${UPDATE_VCPKG}; then
        echo "Bootstrapping vcpkg..."
        "${VCPKG_DIR}/bootstrap-vcpkg.sh" -disableMetrics
    fi

    green "vcpkg is ready."
elif ${CLEAN_CACHE}; then
    yellow "Skipping vcpkg cache cleanup because system dependencies will be used."
fi

# ---------- Configure CMake ----------
echo ""
echo "Configuring CMake..."

CMAKE_ARGS=(
    -B "${BUILD_DIR}"
    -S "${SCRIPT_DIR}"
    -G Ninja
    -DCMAKE_BUILD_TYPE=Release
)

if [[ "${PLATFORM}" == "mac" ]]; then
    CMAKE_ARGS+=(-DCMAKE_OSX_ARCHITECTURES="${ARCH}")
fi

if ${USE_SYSTEM_DEPS}; then
    CMAKE_ARGS+=("${SYSTEM_CMAKE_ARGS[@]}")
else
    CMAKE_ARGS+=(-DCMAKE_TOOLCHAIN_FILE="${VCPKG_DIR}/scripts/buildsystems/vcpkg.cmake")
fi

# Pass through any extra arguments
CMAKE_ARGS+=("${CMAKE_EXTRA_ARGS[@]+"${CMAKE_EXTRA_ARGS[@]}"}")

cmake "${CMAKE_ARGS[@]}"

green ""
green "Configuration complete!"
echo ""
echo "To build:"
echo "  cmake --build build --target PlatypusGui"
echo ""
echo "To run:"
echo "  ./build/PlatypusGui"
echo ""
if ${USE_SYSTEM_DEPS}; then
    echo "Using system-installed Qt6/OpenCV avoided a full vcpkg dependency build."
else
    echo "Subsequent runs of ./configure.sh will be fast (packages cached at ${VCPKG_CACHE_DIR})."
fi
