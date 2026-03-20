#!/bin/bash

load_repo_env() {
  local env_file="$1"

  if [[ -f "${env_file}" ]]; then
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
  fi
}

version_key() {
  local version="$1"
  local major=0
  local minor=0
  local patch=0
  IFS=. read -r major minor patch <<<"${version}"
  printf '%05d%05d%05d\n' "${major:-0}" "${minor:-0}" "${patch:-0}"
}

version_gt() {
  [[ "$(version_key "$1")" > "$(version_key "$2")" ]]
}

is_macho_file() {
  local candidate="$1"
  [[ -f "${candidate}" ]] || return 1
  file -b "${candidate}" 2>/dev/null | grep -q 'Mach-O'
}

extract_minos() {
  local binary_path="$1"
  local minos=""

  minos="$(xcrun vtool -show-build "${binary_path}" 2>/dev/null | awk '/^[[:space:]]*minos / { print $2; exit }')"
  if [[ -n "${minos}" ]]; then
    printf '%s\n' "${minos}"
    return 0
  fi

  minos="$(otool -l "${binary_path}" 2>/dev/null | awk '
    $1 == "cmd" && $2 == "LC_BUILD_VERSION" { mode = "build"; next }
    mode == "build" && $1 == "minos" { print $2; exit }
    $1 == "cmd" && $2 == "LC_VERSION_MIN_MACOSX" { mode = "legacy"; next }
    mode == "legacy" && $1 == "version" { print $2; exit }
  ')"

  if [[ -n "${minos}" ]]; then
    printf '%s\n' "${minos}"
    return 0
  fi

  return 1
}

collect_bundle_macho_files() {
  local bundle_path="$1"
  local candidate=""

  while IFS= read -r -d '' candidate; do
    if is_macho_file "${candidate}"; then
      printf '%s\0' "${candidate}"
    fi
  done < <(find "${bundle_path}/Contents" -type f -print0 | sort -z)
}
