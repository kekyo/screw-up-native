#!/bin/bash
set -euo pipefail

# screw-up-native
# Copyright (c) Kouji Matsui. (@kekyo@mi.kekyo.net)
# Under MIT.
# https://github.com/kekyo/screw-up-native

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIPT_NAME="$(basename "$0")"

usage() {
    cat <<EOF
Usage: ${SCRIPT_NAME} [options]
Options:
  --arch <i386|amd64|all>    Target architecture (default: all)
  --debug                    Debug build
  --libgit2-version <ver>    libgit2 version (default: ${LIBGIT2_VERSION:-1.9.2})
  -h, --help                 Show this help
EOF
}

log() {
    echo "[build-win32] $*"
}

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        log "Command not found: $1"
        exit 1
    fi
}

normalize_arch() {
    case "$1" in
        amd64 | x86_64) echo "amd64" ;;
        i386 | i486 | i586 | i686 | x86) echo "i386" ;;
        all) echo "all" ;;
        *)
            echo ""
            ;;
    esac
}

LIBGIT2_VERSION="${LIBGIT2_VERSION:-1.9.2}"
ARCH_INPUTS=()
DEBUG_BUILD=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --arch)
            ARCH_INPUTS+=("$2")
            shift 2
            ;;
        --debug)
            DEBUG_BUILD=1
            shift
            ;;
        --libgit2-version)
            LIBGIT2_VERSION="$2"
            shift 2
            ;;
        -h | --help)
            usage
            exit 0
            ;;
        *)
            log "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

if [[ ${#ARCH_INPUTS[@]} -eq 0 ]]; then
    ARCH_INPUTS=("all")
fi

require_cmd cmake
require_cmd make
require_cmd pkg-config
require_cmd zip
require_cmd curl
require_cmd tar

JOBS=1
if command -v nproc >/dev/null 2>&1; then
    JOBS="$(nproc)"
fi

ROOT_DIR="${SCRIPT_DIR}"
BUILD_ROOT="${WIN_BUILD_ROOT:-/tmp/screw-up-native-windows-build}"
DEPS_DIR="${BUILD_ROOT}/deps"
ARTIFACTS_ROOT="${ROOT_DIR}/artifacts/windows"
HOST_BUILD_DIR="${BUILD_ROOT}/host"
HOST_BOOTSTRAP="${HOST_BUILD_DIR}/screw-up-bootstrap"

LIBGIT2_VERSION="${LIBGIT2_VERSION#v}"
LIBGIT2_TAG="v${LIBGIT2_VERSION}"
LIBGIT2_URL="https://github.com/libgit2/libgit2/archive/refs/tags/${LIBGIT2_TAG}.tar.gz"
LIBGIT2_TARBALL="${DEPS_DIR}/libgit2-${LIBGIT2_VERSION}.tar.gz"
LIBGIT2_SRC="${DEPS_DIR}/libgit2-${LIBGIT2_VERSION}"

BUILD_TYPE="Release"
BUILD_CFLAGS="-O2"
BUILD_LDFLAGS="-static-libgcc"
if [[ "${DEBUG_BUILD}" -eq 1 ]]; then
    BUILD_TYPE="Debug"
    BUILD_CFLAGS="-O0 -g"
    BUILD_LDFLAGS="-g -static-libgcc"
fi
BUILD_CFLAGS_ALL="-std=c99 ${BUILD_CFLAGS}"
TOOLCHAIN_CC=""
TOOLCHAIN_AR=""
TOOLCHAIN_RANLIB=""
TOOLCHAIN_WINDRES=""
TOOLCHAIN_OBJDUMP=""
TOOLCHAIN_STRIP=""

prepare_libgit2_source() {
    mkdir -p "${DEPS_DIR}"
    if [[ ! -d "${LIBGIT2_SRC}" ]]; then
        if [[ ! -f "${LIBGIT2_TARBALL}" ]]; then
            log "Downloading libgit2 ${LIBGIT2_VERSION}"
            curl -fsSL "${LIBGIT2_URL}" -o "${LIBGIT2_TARBALL}"
        fi
        log "Extracting libgit2 ${LIBGIT2_VERSION}"
        tar -xf "${LIBGIT2_TARBALL}" -C "${DEPS_DIR}"
    fi
}

resolve_toolchain() {
    local triplet="$1"
    TOOLCHAIN_CC="$(command -v "${triplet}-gcc" || true)"
    TOOLCHAIN_AR="$(command -v "${triplet}-ar" || true)"
    TOOLCHAIN_RANLIB="$(command -v "${triplet}-ranlib" || true)"
    TOOLCHAIN_WINDRES="$(command -v "${triplet}-windres" || true)"
    TOOLCHAIN_OBJDUMP="$(command -v "${triplet}-objdump" || true)"
    TOOLCHAIN_STRIP="$(command -v "${triplet}-strip" || true)"

    if [[ -z "${TOOLCHAIN_CC}" || -z "${TOOLCHAIN_AR}" || -z "${TOOLCHAIN_RANLIB}" || -z "${TOOLCHAIN_WINDRES}" || -z "${TOOLCHAIN_OBJDUMP}" ]]; then
        log "Missing toolchain for ${triplet}"
        exit 1
    fi
}

build_host_bootstrap() {
    log "Building host bootstrap"
    rm -rf "${HOST_BUILD_DIR}"
    mkdir -p "${HOST_BUILD_DIR}"

    if ! pkg-config --exists libgit2; then
        log "libgit2 pkg-config not found; ensure libgit2-dev is installed"
        exit 1
    fi

    local host_cflags
    local host_libs
    host_cflags="$(pkg-config --cflags libgit2)"
    host_libs="$(pkg-config --libs libgit2)"

    make -j "${JOBS}" \
        BUILD_DIR="${HOST_BUILD_DIR}" \
        CFLAGS="${BUILD_CFLAGS}" \
        CFLAGS_ALL="${BUILD_CFLAGS_ALL}" \
        CPPFLAGS="${host_cflags}" \
        LDFLAGS="" \
        LIBS="${host_libs}" \
        bootstrap

    if [[ ! -x "${HOST_BOOTSTRAP}" ]]; then
        log "Host bootstrap not found at ${HOST_BOOTSTRAP}"
        exit 1
    fi
}

filter_windows_libs() {
    local input="$1"
    local filtered=()
    local token
    for token in ${input}; do
        case "${token}" in
            -lrt | -lpthread | -ldl)
                ;;
            *)
                filtered+=("${token}")
                ;;
        esac
    done
    printf '%s' "${filtered[*]}"
}

generate_version_header() {
    local output_path="$1"
    mkdir -p "$(dirname "${output_path}")"
    "${HOST_BOOTSTRAP}" format -i "${ROOT_DIR}/src/version.h.in" "${output_path}"
}

get_version_from_bootstrap() {
    local version
    version="$(cd "${ROOT_DIR}" && printf '{version}\n' | "${HOST_BOOTSTRAP}" format --no-wds 2>/dev/null | head -n 1)"
    if [[ -z "${version}" ]]; then
        log "Failed to extract version using bootstrap"
        exit 1
    fi
    echo "${version}"
}

build_libgit2() {
    local arch_dir="$1"
    local processor="$2"
    local build_dir="${BUILD_ROOT}/libgit2-${arch_dir}-build"
    local install_dir="${BUILD_ROOT}/libgit2-${arch_dir}-install"

    rm -rf "${build_dir}" "${install_dir}"
    mkdir -p "${build_dir}"

    cmake -S "${LIBGIT2_SRC}" -B "${build_dir}" \
        -DCMAKE_SYSTEM_NAME=Windows \
        -DCMAKE_SYSTEM_PROCESSOR="${processor}" \
        -DCMAKE_C_COMPILER="${TOOLCHAIN_CC}" \
        -DCMAKE_RC_COMPILER="${TOOLCHAIN_WINDRES}" \
        -DCMAKE_AR="${TOOLCHAIN_AR}" \
        -DCMAKE_RANLIB="${TOOLCHAIN_RANLIB}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DCMAKE_INSTALL_PREFIX="${install_dir}" \
        -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
        -DNEED_LIBRT=0 \
        -DBUILD_SHARED_LIBS=OFF \
        -DBUILD_TESTS=OFF \
        -DBUILD_CLI=OFF \
        -DBUILD_EXAMPLES=OFF \
        -DUSE_SSH=OFF \
        -DUSE_HTTPS=OFF \
        -DUSE_HTTP_PARSER=builtin \
        -DREGEX_BACKEND=builtin \
        -DUSE_BUNDLED_ZLIB=ON

    cmake --build "${build_dir}" -- -j "${JOBS}"
    cmake --install "${build_dir}"
}

is_system_dll() {
    local name_upper
    name_upper="$(echo "$1" | tr '[:lower:]' '[:upper:]')"

    case "${name_upper}" in
        API-MS-WIN-* | EXT-MS-WIN-*)
            return 0
            ;;
    esac

    local sys_dlls=(
        ADVAPI32.DLL
        BCRYPT.DLL
        COMCTL32.DLL
        COMDLG32.DLL
        CRYPT32.DLL
        GDI32.DLL
        IMM32.DLL
        IPHLPAPI.DLL
        KERNEL32.DLL
        KERNELBASE.DLL
        MSVCRT.DLL
        NTDLL.DLL
        OLE32.DLL
        OLEAUT32.DLL
        RPCRT4.DLL
        SECUR32.DLL
        SHELL32.DLL
        SHLWAPI.DLL
        USER32.DLL
        VERSION.DLL
        WINMM.DLL
        WS2_32.DLL
    )

    local sys
    for sys in "${sys_dlls[@]}"; do
        if [[ "${name_upper}" == "${sys}" ]]; then
            return 0
        fi
    done
    return 1
}

find_dll() {
    local dll_name="$1"
    shift
    local search_dir
    for search_dir in "$@"; do
        if [[ -d "${search_dir}" ]]; then
            local found
            found="$(find "${search_dir}" -type f -iname "${dll_name}" -print -quit)"
            if [[ -n "${found}" ]]; then
                echo "${found}"
                return 0
            fi
        fi
    done
    return 1
}

copy_runtime_deps() {
    local stage_dir="$1"
    local root_file="$2"
    local extra_root="$3"
    local sysroot

    require_cmd "${TOOLCHAIN_OBJDUMP}"
    sysroot="$("${TOOLCHAIN_CC}" -print-sysroot)"

    local search_dirs=()
    if [[ -n "${extra_root}" ]]; then
        search_dirs+=("${extra_root}/bin" "${extra_root}/lib")
    fi
    search_dirs+=("${sysroot}")

    declare -A copied=()
    local queue=("${root_file}")

    while [[ ${#queue[@]} -gt 0 ]]; do
        local file="${queue[0]}"
        queue=("${queue[@]:1}")

        local dlls
        dlls="$("${TOOLCHAIN_OBJDUMP}" -p "${file}" | sed -n 's/.*DLL Name: //p')"
        while IFS= read -r dll; do
            if [[ -z "${dll}" ]]; then
                continue
            fi
            if is_system_dll "${dll}"; then
                continue
            fi
            local key
            key="$(echo "${dll}" | tr '[:lower:]' '[:upper:]')"
            if [[ -n "${copied[${key}]:-}" ]]; then
                continue
            fi

            local dll_path
            if ! dll_path="$(find_dll "${dll}" "${search_dirs[@]}")"; then
                log "Missing dependency: ${dll} (required by $(basename "${file}"))"
                exit 1
            fi

            cp -f "${dll_path}" "${stage_dir}/"
            copied["${key}"]=1
            queue+=("${dll_path}")
        done <<< "${dlls}"
    done
}

build_screw_up() {
    local arch_dir="$1"

    local libgit2_prefix="${BUILD_ROOT}/libgit2-${arch_dir}-install"
    local target_build_dir="${BUILD_ROOT}/${arch_dir}"
    local version_header="${target_build_dir}/generated/version.h"

    rm -rf "${target_build_dir}"
    mkdir -p "${target_build_dir}"
    generate_version_header "${version_header}"
    local version
    version="$(get_version_from_bootstrap)"

    local libgit2_pkg_config="${libgit2_prefix}/lib/pkgconfig"
    if [[ ! -d "${libgit2_pkg_config}" ]]; then
        log "libgit2 pkg-config directory not found: ${libgit2_pkg_config}"
        exit 1
    fi

    local libgit2_cflags
    local libgit2_libs
    libgit2_cflags="$(PKG_CONFIG_LIBDIR="${libgit2_pkg_config}" pkg-config --cflags libgit2)"
    libgit2_libs="$(PKG_CONFIG_LIBDIR="${libgit2_pkg_config}" pkg-config --static --libs libgit2)"
    libgit2_libs="$(filter_windows_libs "${libgit2_libs}")"

    make -j "${JOBS}" \
        BUILD_DIR="${target_build_dir}" \
        USE_BOOTSTRAP=0 \
        VERSION_HEADER="${version_header}" \
        CC="${TOOLCHAIN_CC}" \
        CFLAGS="${BUILD_CFLAGS}" \
        CFLAGS_ALL="${BUILD_CFLAGS_ALL}" \
        CPPFLAGS="${libgit2_cflags}" \
        LDFLAGS="${BUILD_LDFLAGS}" \
        LIBS="${libgit2_libs}"

    local exe_path=""
    if [[ -f "${target_build_dir}/screw-up.exe" ]]; then
        exe_path="${target_build_dir}/screw-up.exe"
    elif [[ -f "${target_build_dir}/screw-up" ]]; then
        exe_path="${target_build_dir}/screw-up"
    else
        log "screw-up binary not found in ${target_build_dir}"
        exit 1
    fi

    if [[ -n "${TOOLCHAIN_STRIP}" ]]; then
        "${TOOLCHAIN_STRIP}" --strip-unneeded "${exe_path}" || true
    fi

    local stage_dir="${ARTIFACTS_ROOT}/${arch_dir}"
    rm -rf "${stage_dir}"
    mkdir -p "${stage_dir}"
    cp -f "${exe_path}" "${stage_dir}/screw-up.exe"
    copy_runtime_deps "${stage_dir}" "${exe_path}" "${libgit2_prefix}"

    if [[ ! -f "${LIBGIT2_SRC}/COPYING" ]]; then
        log "libgit2 COPYING not found at ${LIBGIT2_SRC}/COPYING"
        exit 1
    fi
    if [[ ! -f "${ROOT_DIR}/LICENSE" ]]; then
        log "LICENSE not found at ${ROOT_DIR}/LICENSE"
        exit 1
    fi
    cp -f "${LIBGIT2_SRC}/COPYING" "${stage_dir}/COPYING"
    cp -f "${ROOT_DIR}/LICENSE" "${stage_dir}/LICENSE"

    local zip_path="${ROOT_DIR}/artifacts/screw-up-native-windows-${arch_dir}-${version}.zip"
    rm -f "${zip_path}"
    (cd "${stage_dir}" && zip -r "${zip_path}" .)
    log "Package created: ${zip_path}"
}

prepare_libgit2_source
build_host_bootstrap

for arch_input in "${ARCH_INPUTS[@]}"; do
    arch_raw="${arch_input}"
    arch_input="$(normalize_arch "${arch_input}")"
    if [[ -z "${arch_input}" ]]; then
        log "Unsupported arch: ${arch_raw}"
        exit 1
    fi
    if [[ "${arch_input}" == "all" ]]; then
        ARCH_INPUTS=("i386" "amd64")
        break
    fi
done

for arch_input in "${ARCH_INPUTS[@]}"; do
    arch_raw="${arch_input}"
    arch_input="$(normalize_arch "${arch_input}")"
    case "${arch_input}" in
        i386)
            TRIPLET="i686-w64-mingw32"
            PROCESSOR="i686"
            ;;
        amd64)
            TRIPLET="x86_64-w64-mingw32"
            PROCESSOR="x86_64"
            ;;
        *)
            log "Unsupported arch: ${arch_raw}"
            exit 1
            ;;
    esac

    resolve_toolchain "${TRIPLET}"

    log "Building libgit2 for ${arch_input}"
    build_libgit2 "${arch_input}" "${PROCESSOR}"

    log "Building screw-up for ${arch_input}"
    build_screw_up "${arch_input}"
done
