#!/usr/bin/env bash
set -euo pipefail

# ---------------------------------------------------------------------------
# build_android.sh — Configure and build the Calaos Home Android app (Qt 6 + CMake + Ninja)
# Usage: ./build_android.sh [Debug|Release] [--lib-only]
#   --lib-only  build only the native library (skip Gradle/APK packaging)
# Environment overrides: JAVA_HOME, ANDROID_SDK_ROOT, ANDROID_NDK_ROOT, QT_ROOT,
#   ANDROID_OPENSSL_DIR, CALAOS_ANDROID_ABIS
# ---------------------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build-android"

CONFIG="${1:-Debug}"
LIB_ONLY=false
[[ "${2:-}" == "--lib-only" ]] && LIB_ONLY=true

QT_ROOT="${QT_ROOT:-$HOME/Qt/6.11.0}"
QT_ANDROID="${QT_ANDROID:-${QT_ROOT}/android_arm64_v8a}"
QT_HOST="${QT_HOST:-${QT_ROOT}/macos}"

export ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-$HOME/Library/Android/sdk}"
export ANDROID_NDK_ROOT="${ANDROID_NDK_ROOT:-${ANDROID_SDK_ROOT}/ndk/27.2.12479018}"
if [[ -z "${JAVA_HOME:-}" ]] && command -v brew >/dev/null 2>&1; then
    JAVA_HOME="$(brew --prefix openjdk@17 2>/dev/null)/libexec/openjdk.jdk/Contents/Home"
fi
export JAVA_HOME="${JAVA_HOME:-}"
ANDROID_OPENSSL_DIR="${ANDROID_OPENSSL_DIR:-${ANDROID_SDK_ROOT}/android_openssl}"
CALAOS_ANDROID_ABIS="${CALAOS_ANDROID_ABIS:-arm64-v8a}"

# ninja: Homebrew, or the one shipped with the Qt installer (~/Qt/Tools/Ninja)
export PATH="/opt/homebrew/bin:${QT_ROOT%/*}/Tools/Ninja:$PATH"

fail() { echo "ERROR: $1" >&2; exit 1; }

[[ "${CONFIG}" == "Debug" || "${CONFIG}" == "Release" ]] || fail "config must be Debug or Release"
[[ -d "${QT_ANDROID}" ]] || fail "Qt Android kit not found: ${QT_ANDROID}"
[[ -d "${QT_HOST}" ]] || fail "Qt host (macOS) kit not found: ${QT_HOST}"
[[ -d "${ANDROID_SDK_ROOT}" ]] || fail "Android SDK not found: ${ANDROID_SDK_ROOT}"
[[ -f "${ANDROID_NDK_ROOT}/build/cmake/android.toolchain.cmake" ]] || fail "Android NDK not found: ${ANDROID_NDK_ROOT}"
[[ -x "${JAVA_HOME}/bin/java" ]] || fail "JDK 17 not found (JAVA_HOME='${JAVA_HOME}'); brew install openjdk@17"
[[ -f "${ANDROID_OPENSSL_DIR}/android_openssl.cmake" ]] || fail "KDAB android_openssl not found: ${ANDROID_OPENSSL_DIR}"
command -v ninja >/dev/null 2>&1 || fail "ninja not found in PATH"
command -v cmake >/dev/null 2>&1 || fail "cmake not found in PATH"

echo "==> Configuring for Android (${CONFIG}, ABIs: ${CALAOS_ANDROID_ABIS})"
cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE="${CONFIG}" \
    -DCMAKE_TOOLCHAIN_FILE="${ANDROID_NDK_ROOT}/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-28 \
    -DANDROID_SDK_ROOT="${ANDROID_SDK_ROOT}" \
    -DANDROID_NDK_ROOT="${ANDROID_NDK_ROOT}" \
    -DANDROID_OPENSSL_DIR="${ANDROID_OPENSSL_DIR}" \
    -DCALAOS_ANDROID_ABIS="${CALAOS_ANDROID_ABIS}" \
    -DCMAKE_PREFIX_PATH="${QT_ANDROID}" \
    -DQT_HOST_PATH="${QT_HOST}" \
    -DQT_ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT}" \
    -DQT_ANDROID_NDK_ROOT="${ANDROID_NDK_ROOT}" \
    -DJAVA_HOME="${JAVA_HOME}" \
    -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5

echo ""
echo "==> Building (${CONFIG})"
if [[ "${LIB_ONLY}" == true ]]; then
    cmake --build "${BUILD_DIR}" --target CalaosHome
else
    cmake --build "${BUILD_DIR}"
fi

echo ""
echo "==> Done. Native library / APK:"
find "${BUILD_DIR}" \( -name 'libCalaosHome*.so' -o -name '*.apk' \) 2>/dev/null | head -5
