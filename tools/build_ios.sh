#!/usr/bin/env bash
set -euo pipefail

# ---------------------------------------------------------------------------
# build_ios.sh — Configure and build the Calaos Home iOS app with CMake/Xcode
# Usage: ./build_ios.sh [Debug|Release] [--archive]
# Environment overrides: QT_VERSION (default 6.12.0), QT_ROOT (default ~/Qt/$QT_VERSION),
#   QT_IOS (Qt iOS kit), QT_HOST (Qt macOS host kit), IOS_DEPLOYMENT_TARGET
# Rolling back to another installed Qt: QT_VERSION=6.11.0 ./tools/build_ios.sh Debug
#   (a cache configured against another kit is cleared automatically).
# ---------------------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() { echo "ERROR: $1" >&2; exit 1; }

CONFIG="${1:-Debug}"
ARCHIVE=false
if [[ "${2:-}" == "--archive" ]]; then
    ARCHIVE=true
fi

# Qt version: the only place to bump it.
QT_VERSION="${QT_VERSION:-6.12.0}"
QT_ROOT="${QT_ROOT:-$HOME/Qt/${QT_VERSION}}"
TEAM_ID="9WYC46992U"

# Target SDK. Only device builds (iphoneos) are wired today; simulator builds
# (iphonesimulator, Qt kit ios_simulator_arm64) are added by IOS-1.
IOS_SDK="iphoneos"

# Qt >= 6.12 ships one kit per SDK; Qt <= 6.11 has a single "ios" kit.
qt_ios_kit() {
    case "$1" in
        iphoneos) echo ios_device ;;
        iphonesimulator) echo ios_simulator_arm64 ;;
        *) return 1 ;;
    esac
}
build_dir_for_sdk() {
    case "$1" in
        iphoneos) echo "${SCRIPT_DIR}/build-ios" ;;
        iphonesimulator) echo "${SCRIPT_DIR}/build-ios-sim" ;;
        *) return 1 ;;
    esac
}

if [[ -z "${QT_IOS:-}" ]]; then
    QT_IOS="${QT_ROOT}/$(qt_ios_kit "${IOS_SDK}")"
    if [[ ! -d "${QT_IOS}" && -d "${QT_ROOT}/ios" ]]; then
        QT_IOS="${QT_ROOT}/ios"
    fi
fi
QT_HOST="${QT_HOST:-${QT_ROOT}/macos}"
BUILD_DIR="$(build_dir_for_sdk "${IOS_SDK}")"

# Validate Qt paths
[[ -d "${QT_IOS}" ]] || fail "Qt iOS kit not found: ${QT_IOS}"
[[ -d "${QT_HOST}" ]] || fail "Qt host (macOS) kit not found: ${QT_HOST}"

# Minimum iOS version. It cannot be lower than the one the Qt kit is built for
# (CMAKE_OSX_DEPLOYMENT_TARGET in its qt.toolchain.cmake: 18 for Qt 6.12, 17 for 6.11).
IOS_DEPLOYMENT_TARGET="${IOS_DEPLOYMENT_TARGET:-18.0}"
qt_ios_min="$(sed -n 's/^set(CMAKE_OSX_DEPLOYMENT_TARGET "\([0-9.]*\)".*/\1/p' \
    "${QT_IOS}/lib/cmake/Qt6/qt.toolchain.cmake" 2>/dev/null | head -1)"
if [[ -n "${qt_ios_min}" && "${IOS_DEPLOYMENT_TARGET%%.*}" -lt "${qt_ios_min%%.*}" ]]; then
    fail "IOS_DEPLOYMENT_TARGET=${IOS_DEPLOYMENT_TARGET} is below the Qt kit minimum (iOS ${qt_ios_min})"
fi

# A cache configured against another Qt kit (version bump/rollback) keeps the old
# Qt6_DIR and SDK paths: start from a clean cache in that case.
if [[ -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
    cached_qt="$(sed -n 's/^Qt6_DIR:[A-Z]*=//p' "${BUILD_DIR}/CMakeCache.txt")"
    if [[ -n "${cached_qt}" && "${cached_qt}" != "${QT_IOS}/"* ]]; then
        echo "==> Qt kit changed (cache: ${cached_qt}): clearing the CMake cache"
        rm -rf "${BUILD_DIR}/CMakeCache.txt" "${BUILD_DIR}/CMakeFiles"
    fi
fi

echo "==> Configuring for iOS (${CONFIG}, SDK: ${IOS_SDK}, min iOS ${IOS_DEPLOYMENT_TARGET})"
echo "    Qt kit: ${QT_IOS}"
echo "    Qt host: ${QT_HOST}"
cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
    -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="${IOS_DEPLOYMENT_TARGET}" \
    -DCMAKE_PREFIX_PATH="${QT_IOS}" \
    -DQT_HOST_PATH="${QT_HOST}" \
    -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM="${TEAM_ID}"

# CMake's Xcode generator leaves "shadow" symlinks in the product dirs
# (<CONFIG>-<sdk>/) pointing at the actual build products. A regular build and
# an archive use different product locations (DerivedData vs ArchiveIntermediates),
# so a symlink left by a previous run points at a stale location and the next build
# fails with "unable to create directory .../CalaosHome.app/PlugIns". Remove these
# stale symlinks before every build. (Real dirs are left alone — the build reuses them.)
clean_stale_symlinks() {
    local d
    for d in "${BUILD_DIR}/${CONFIG}-${IOS_SDK}" "${BUILD_DIR}/ios/${CONFIG}-${IOS_SDK}"; do
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
    -- -allowProvisioningUpdates -parallelizeTargets -jobs "$(sysctl -n hw.logicalcpu)"

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
