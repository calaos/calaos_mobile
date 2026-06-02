#!/usr/bin/env bash
set -euo pipefail

# ---------------------------------------------------------------------------
# build_ios.sh — Configure and build the Calaos Home iOS app with CMake/Xcode
# Usage: ./build_ios.sh [Debug|Release] [--archive]
# ---------------------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build-ios"

CONFIG="${1:-Debug}"
ARCHIVE=false
if [[ "${2:-}" == "--archive" ]]; then
    ARCHIVE=true
fi

QT_IOS="/Users/raoul/Qt/6.11.0/ios"
QT_HOST="/Users/raoul/Qt/6.11.0/macos"
TEAM_ID="9WYC46992U"

# Validate Qt paths
if [[ ! -d "${QT_IOS}" ]]; then
    echo "ERROR: Qt iOS path not found: ${QT_IOS}" >&2
    exit 1
fi
if [[ ! -d "${QT_HOST}" ]]; then
    echo "ERROR: Qt host (macOS) path not found: ${QT_HOST}" >&2
    exit 1
fi

echo "==> Configuring for iOS (${CONFIG})"
cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
    -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
    -DCMAKE_PREFIX_PATH="${QT_IOS}" \
    -DQT_HOST_PATH="${QT_HOST}" \
    -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
    -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM="${TEAM_ID}"

# CMake's Xcode generator leaves "shadow" symlinks in the product dirs
# (<CONFIG>-iphoneos/) pointing at the actual build products. A regular build and
# an archive use different product locations (DerivedData vs ArchiveIntermediates),
# so a symlink left by a previous run points at a stale location and the next build
# fails with "unable to create directory .../CalaosHome.app/PlugIns". Remove these
# stale symlinks before every build. (Real dirs are left alone — the build reuses them.)
clean_stale_symlinks() {
    local d
    for d in "${BUILD_DIR}/${CONFIG}-iphoneos" "${BUILD_DIR}/ios/${CONFIG}-iphoneos"; do
        [[ -d "$d" ]] || continue
        find "$d" -maxdepth 1 -type l \
            \( -name "*.app" -o -name "*.appex" -o -name "*.app.dSYM" -o -name "*.appex.dSYM" \) \
            -print -delete | sed 's/^/Removed stale symlink: /'
    done
}

echo ""
echo "==> Building (${CONFIG})"
clean_stale_symlinks
cmake --build "${BUILD_DIR}" \
    --config "${CONFIG}" \
    -- -parallelizeTargets -jobs "$(sysctl -n hw.logicalcpu)"

if [[ "${ARCHIVE}" == true ]]; then
    echo ""
    echo "==> Archiving"
    clean_stale_symlinks
    cmake --build "${BUILD_DIR}" \
        --config "${CONFIG}" \
        --target archive

    # Register the archive with Xcode's Organizer by copying it into
    # ~/Library/Developer/Xcode/Archives/<date>/. Xcode only lists archives
    # found there; the one xcodebuild wrote into the build tree is invisible to it.
    _src_archive="${BUILD_DIR}/CalaosMobile.xcarchive"
    if [[ -d "${_src_archive}" ]]; then
        _organizer_dir="${HOME}/Library/Developer/Xcode/Archives/$(date +%Y-%m-%d)"
        _dest_archive="${_organizer_dir}/CalaosHome-$(date +%H%M%S).xcarchive"
        mkdir -p "${_organizer_dir}"
        cp -R "${_src_archive}" "${_dest_archive}"
        echo "==> Archive added to Xcode Organizer: ${_dest_archive}"
    fi
fi

echo ""
echo "==> Done. App bundle:"
find "${BUILD_DIR}" -name "CalaosHome.app" -maxdepth 6 2>/dev/null | head -3
