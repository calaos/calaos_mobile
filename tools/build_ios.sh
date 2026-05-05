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

echo ""
echo "==> Building (${CONFIG})"
cmake --build "${BUILD_DIR}" \
    --config "${CONFIG}" \
    -- -parallelizeTargets -jobs "$(sysctl -n hw.logicalcpu)"

if [[ "${ARCHIVE}" == true ]]; then
    echo ""
    echo "==> Archiving"
    # The regular build (cmake --build) creates symlinks in Release-iphoneos/ pointing
    # into DerivedData. xcodebuild -archive then tries to mkdir over those symlinks and
    # fails. Remove them before running the archive target.
    _release_dir="${BUILD_DIR}/Release-iphoneos"
    for f in "${_release_dir}/CalaosHome.app" "${_release_dir}/CalaosHome.app.dSYM"; do
        [[ -L "$f" ]] && rm "$f" && echo "Removed stale symlink: $f"
    done
    cmake --build "${BUILD_DIR}" \
        --config "${CONFIG}" \
        --target archive
fi

echo ""
echo "==> Done. App bundle:"
find "${BUILD_DIR}" -name "CalaosHome.app" -maxdepth 6 2>/dev/null | head -3
