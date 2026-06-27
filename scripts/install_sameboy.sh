#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

SAMEBOY_VERSION="${SAMEBOY_VERSION:-v1.0.3}"
SDL_VERSION="${SDL_VERSION:-2.32.4}"
PREFIX="${PREFIX:-${HOME}/.local}"
WORK_DIR="${WORK_DIR:-${REPO_ROOT}/.sameboy-build}"
KEEP_WORK="${KEEP_WORK:-1}"

SAMEBOY_VERSION_NUMBER="${SAMEBOY_VERSION#v}"
SAMEBOY_ARCHIVE="${WORK_DIR}/sameboy-${SAMEBOY_VERSION}.tar.gz"
SAMEBOY_DIR="${WORK_DIR}/SameBoy-${SAMEBOY_VERSION_NUMBER}"
SAMEBOY_URL="https://github.com/LIJI32/SameBoy/archive/refs/tags/${SAMEBOY_VERSION}.tar.gz"

SDL_ARCHIVE="${WORK_DIR}/SDL2-${SDL_VERSION}.tar.gz"
SDL_DIR="${WORK_DIR}/SDL2-${SDL_VERSION}"
SDL_URL="https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/SDL2-${SDL_VERSION}.tar.gz"

PKG_CONFIG_DIR="${WORK_DIR}/pkgconfig"
INSTALL_BIN="${PREFIX}/bin"
INSTALL_DATA="${PREFIX}/share/sameboy"
MARKER_FILE="${WORK_DIR}/.sameboy-installer-workdir"

die() {
    printf '%s\n' "$*" >&2
    exit 1
}

work_dir_has_files() {
    [ -n "$(find "${WORK_DIR}" -mindepth 1 -maxdepth 1 -print -quit 2>/dev/null)" ]
}

work_dir_is_safe() {
    case "${WORK_DIR%/}" in
        ""|"/"|"/tmp"|"/var/tmp"|"${HOME}"|"${REPO_ROOT}")
            return 1
            ;;
    esac

    return 0
}

prepare_work_dir() {
    if ! work_dir_is_safe; then
        die "Refusing unsafe work directory: ${WORK_DIR}"
    fi

    if [ -e "${WORK_DIR}" ] && [ ! -f "${MARKER_FILE}" ] && work_dir_has_files; then
        die "Refusing to use non-empty work directory without installer marker: ${WORK_DIR}"
    fi

    mkdir -p "${WORK_DIR}" "${PKG_CONFIG_DIR}"
    : > "${MARKER_FILE}"
}

cleanup() {
    if [ "${KEEP_WORK}" != "1" ] && [ -f "${MARKER_FILE}" ] && work_dir_is_safe; then
        rm -rf "${WORK_DIR}"
    elif [ "${KEEP_WORK}" = "1" ]; then
        printf 'Kept build workspace: %s\n' "${WORK_DIR}"
    fi
}
trap cleanup EXIT

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        printf 'Missing required command: %s\n' "$1" >&2
        exit 1
    fi
}

download() {
    local url="$1"
    local output="$2"

    if [ ! -f "${output}" ]; then
        printf 'Downloading %s\n' "${url}"
        curl -L "${url}" -o "${output}"
    fi
}

find_sdl_runtime() {
    local runtime

    runtime="$(ldconfig -p 2>/dev/null | awk '/libSDL2-2\.0\.so\.0 / { print $NF; exit }')"
    if [ -n "${runtime}" ]; then
        printf '%s\n' "${runtime}"
        return
    fi

    if [ -f /usr/lib/x86_64-linux-gnu/libSDL2-2.0.so.0 ]; then
        printf '%s\n' /usr/lib/x86_64-linux-gnu/libSDL2-2.0.so.0
        return
    fi

    if [ -f /lib/x86_64-linux-gnu/libSDL2-2.0.so.0 ]; then
        printf '%s\n' /lib/x86_64-linux-gnu/libSDL2-2.0.so.0
        return
    fi

    printf 'Could not find libSDL2-2.0.so.0. Install the SDL2 runtime first.\n' >&2
    exit 1
}

patch_sameboy_source() {
    if ! grep -q '#include <math.h>' "${SAMEBOY_DIR}/SDL/main.c"; then
        sed -i '/#include <ctype.h>/a #include <math.h>' "${SAMEBOY_DIR}/SDL/main.c"
    fi

    sed -i \
        's/SDL_Init(SDL_INIT_EVERYTHING & ~SDL_INIT_AUDIO)/SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER)/' \
        "${SAMEBOY_DIR}/SDL/main.c"
}

install_sameboy() {
    local build_sdl_dir="${SAMEBOY_DIR}/build/bin/SDL"

    install -d "${INSTALL_BIN}" "${INSTALL_DATA}"
    install -m 755 "${build_sdl_dir}/sameboy" "${INSTALL_BIN}/sameboy"

    (
        cd "${build_sdl_dir}"
        tar --exclude='./sameboy' -cf - .
    ) | (
        cd "${INSTALL_DATA}"
        tar -xf -
    )
}

main() {
    local sdl_runtime

    require_cmd curl
    require_cmd tar
    require_cmd make
    require_cmd pkg-config
    require_cmd rgbasm
    require_cmd rgblink
    require_cmd rgbgfx

    prepare_work_dir
    sdl_runtime="$(find_sdl_runtime)"

    download "${SAMEBOY_URL}" "${SAMEBOY_ARCHIVE}"
    download "${SDL_URL}" "${SDL_ARCHIVE}"

    tar -xf "${SAMEBOY_ARCHIVE}" -C "${WORK_DIR}"
    tar -xf "${SDL_ARCHIVE}" -C "${WORK_DIR}"

    cat > "${PKG_CONFIG_DIR}/sdl2.pc" <<EOF
prefix=${SDL_DIR}
Name: sdl2
Description: Simple DirectMedia Layer
Version: ${SDL_VERSION}
Cflags: -I${SDL_DIR}/include -I${SDL_DIR}/include/SDL2 -D_REENTRANT
Libs: ${sdl_runtime} -lpthread
EOF

    patch_sameboy_source

    PKG_CONFIG_PATH="${PKG_CONFIG_DIR}${PKG_CONFIG_PATH:+:${PKG_CONFIG_PATH}}" \
        make -C "${SAMEBOY_DIR}" CONF=release PREFIX="${PREFIX}" sdl

    install_sameboy

    printf 'Installed SameBoy %s to %s/sameboy\n' "${SAMEBOY_VERSION}" "${INSTALL_BIN}"
    printf 'Resource files installed to %s\n' "${INSTALL_DATA}"
    printf 'Run this project with: EMU=%s/sameboy make run\n' "${INSTALL_BIN}"
}

main "$@"
