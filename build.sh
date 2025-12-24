#!/bin/bash
set -euo pipefail

usage() {
    echo "Usage: ${BASH_SOURCE[0]} [-d]" >&2
    echo "  -d  Debug build" >&2
}

EXTRA_CFLAGS="${EXTRA_CFLAGS:-}"
EXTRA_CFLAGS_ALL="${EXTRA_CFLAGS_ALL:-}"
EXTRA_CPPFLAGS="${EXTRA_CPPFLAGS:-}"
EXTRA_LDFLAGS="${EXTRA_LDFLAGS:-}"
BUNDLE_LIBGIT2_PREFIX="${BUNDLE_LIBGIT2_PREFIX:-}"
BUNDLE_LIBGIT2_LIBDIR="${BUNDLE_LIBGIT2_LIBDIR:-}"
DPKG_SHLIBDEPS_ARGS="${DPKG_SHLIBDEPS_ARGS:-}"
LIBGIT2_COPYING="${LIBGIT2_COPYING:-}"
PKG_CONFIG_CMD="${PKG_CONFIG_CMD:-pkg-config}"
PKG_CONFIG_BIN="${PKG_CONFIG_CMD%% *}"

BUILD_TYPE="Release"
while getopts ":d" opt; do
    case "${opt}" in
        d)
            BUILD_TYPE="Debug"
            ;;
        *)
            usage
            exit 1
            ;;
    esac
done
shift $((OPTIND - 1))

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ARCH="${ARCH:-$(dpkg --print-architecture)}"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build}"

echo "Building for ARCH=${ARCH}, BUILD_TYPE=${BUILD_TYPE}"

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

BUILD_CFLAGS="-O2"
BUILD_LDFLAGS=""
if [[ "${BUILD_TYPE}" == "Debug" ]]; then
    BUILD_CFLAGS="-O0 -g"
    BUILD_LDFLAGS="-g"
fi
if [[ -n "${EXTRA_CFLAGS}" ]]; then
    BUILD_CFLAGS="${BUILD_CFLAGS} ${EXTRA_CFLAGS}"
fi
if [[ -n "${EXTRA_LDFLAGS}" ]]; then
    BUILD_LDFLAGS="${BUILD_LDFLAGS} ${EXTRA_LDFLAGS}"
fi
BUILD_CFLAGS_ALL="-std=c99 ${BUILD_CFLAGS}"
if [[ -n "${EXTRA_CFLAGS_ALL}" ]]; then
    BUILD_CFLAGS_ALL="${BUILD_CFLAGS_ALL} ${EXTRA_CFLAGS_ALL}"
fi

LIBGIT2_CFLAGS=""
LIBGIT2_LIBS="-lgit2"
if command -v "${PKG_CONFIG_BIN}" >/dev/null 2>&1; then
    if ${PKG_CONFIG_CMD} --exists libgit2; then
        LIBGIT2_CFLAGS="$(${PKG_CONFIG_CMD} --cflags libgit2)"
        LIBGIT2_LIBS="$(${PKG_CONFIG_CMD} --libs libgit2)"
    fi
fi
if [[ -n "${EXTRA_CPPFLAGS}" ]]; then
    LIBGIT2_CFLAGS="${LIBGIT2_CFLAGS} ${EXTRA_CPPFLAGS}"
fi

MAKE_ARGS=(
    "BUILD_DIR=${BUILD_DIR}"
    "CFLAGS=${BUILD_CFLAGS}"
    "CFLAGS_ALL=${BUILD_CFLAGS_ALL}"
    "CPPFLAGS=${LIBGIT2_CFLAGS}"
    "LDFLAGS=${BUILD_LDFLAGS}"
    "LIBS=${LIBGIT2_LIBS}"
)

(
    cd "${ROOT_DIR}"
    make -j "${MAKE_ARGS[@]}"
    make -j "${MAKE_ARGS[@]}" test
)

if [[ ! -x "${BUILD_DIR}/screw-up" ]]; then
    echo "screw-up binary not found at ${BUILD_DIR}/screw-up" >&2
    exit 1
fi

VERSION="$(printf '{version}\n' | "${BUILD_DIR}/screw-up" format --no-wds 2>/dev/null | head -n 1)"
if [[ -z "${VERSION}" ]]; then
    echo "Failed to extract version using screw-up" >&2
    exit 1
fi

if command -v strip >/dev/null 2>&1; then
    strip --strip-unneeded "${BUILD_DIR}/screw-up"
else
    echo "strip command not found; cannot strip binary before packaging" >&2
    exit 1
fi

PKG_ROOT="${BUILD_DIR}/deb"
rm -rf "${PKG_ROOT}"
mkdir -p "${PKG_ROOT}"

CLI_ROOT="${PKG_ROOT}/screw-up-native"
mkdir -p "${CLI_ROOT}/DEBIAN" "${CLI_ROOT}/usr/bin"
cp "${BUILD_DIR}/screw-up" "${CLI_ROOT}/usr/bin/"

DOC_DIR="${CLI_ROOT}/usr/share/doc/screw-up-native"
mkdir -p "${DOC_DIR}"
if [[ -f "${ROOT_DIR}/LICENSE" ]]; then
    cp -a "${ROOT_DIR}/LICENSE" "${DOC_DIR}/LICENSE"
else
    echo "LICENSE file not found at ${ROOT_DIR}/LICENSE" >&2
    exit 1
fi
if [[ -n "${LIBGIT2_COPYING}" ]]; then
    if [[ ! -f "${LIBGIT2_COPYING}" ]]; then
        echo "libgit2 COPYING not found at ${LIBGIT2_COPYING}" >&2
        exit 1
    fi
    cp -a "${LIBGIT2_COPYING}" "${DOC_DIR}/COPYING"
fi

bundle_dir=""
if [[ -n "${BUNDLE_LIBGIT2_PREFIX}" ]]; then
    libdir="${BUNDLE_LIBGIT2_LIBDIR:-${BUNDLE_LIBGIT2_PREFIX}/lib}"
    if [[ ! -d "${libdir}" ]]; then
        echo "libgit2 libdir not found: ${libdir}" >&2
        exit 1
    fi
    bundle_dir="${CLI_ROOT}/usr/lib/screw-up-native"
    mkdir -p "${bundle_dir}"
    shopt -s nullglob
    libs=("${libdir}/libgit2.so"*)
    shopt -u nullglob
    if [[ ${#libs[@]} -eq 0 ]]; then
        echo "libgit2 shared library not found under ${libdir}" >&2
        exit 1
    fi
    cp -a "${libs[@]}" "${bundle_dir}/"
fi

SHLIBDEPS_TMP="$(mktemp -d)"
trap 'rm -rf "${SHLIBDEPS_TMP}"' EXIT
mkdir -p "${SHLIBDEPS_TMP}/debian"
cat > "${SHLIBDEPS_TMP}/debian/control" <<EOF
Source: screw-up-native
Section: utils
Priority: optional
Maintainer: screw-up-native
Standards-Version: 4.6.2

Package: screw-up-native
Architecture: ${ARCH}
Description: Git versioning metadata dumper/formatter
EOF

read -r -a dpkg_shlibdeps_args <<< "${DPKG_SHLIBDEPS_ARGS}"
if [[ -n "${bundle_dir}" ]]; then
    dpkg_shlibdeps_args+=("--ignore-missing-info" "-l${bundle_dir}")
fi
CLI_DEPS="$(cd "${SHLIBDEPS_TMP}" && dpkg-shlibdeps "${dpkg_shlibdeps_args[@]}" -O "${CLI_ROOT}/usr/bin/screw-up" | sed -n 's/^shlibs:Depends=//p')"
if [[ -z "${CLI_DEPS}" ]]; then
    echo "dpkg-shlibdeps failed to calculate runtime dependencies" >&2
    exit 1
else
    echo "Calculated runtime deps: ${CLI_DEPS}"
fi

cat > "${CLI_ROOT}/DEBIAN/control" <<EOF
Package: screw-up-native
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: ${ARCH}
Maintainer: screw-up-native
Depends: ${CLI_DEPS}
Description: Git versioning metadata dumper/formatter
EOF

dpkg-deb --build "${CLI_ROOT}"

echo "Packages created under ${PKG_ROOT}:"
ls -1 "${PKG_ROOT}"/*.deb
