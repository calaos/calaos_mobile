#!/usr/bin/env bash
set -euo pipefail

# ---------------------------------------------------------------------------
# build_android.sh — Configure and build the Calaos Home Android app (Qt 6 + CMake + Ninja)
# Usage: ./build_android.sh [Debug|Release] [--lib-only]
#   --lib-only  build only the native library (skip Gradle/APK packaging)
# Environment overrides: QT_VERSION (default 6.12.0), QT_ROOT (default ~/Qt/$QT_VERSION),
#   QT_ANDROID, QT_HOST, JAVA_HOME, ANDROID_SDK_ROOT, ANDROID_NDK_ROOT,
#   ANDROID_OPENSSL_DIR, CALAOS_ANDROID_ABIS
# Rolling back to another installed Qt is NOT a knob-only operation on Android:
#   android/build.gradle + AndroidManifest.xml are ported from one Qt kit's templates
#   and its AGP needs that kit's Gradle wrapper (6.12 template: AGP 9.2.1, needs
#   Gradle >= 9.4.1; the 6.11 kit ships Gradle 9.3.1). For Qt 6.11: revert the commit
#   "build: port the Qt 6.12 Android templates (QT-1)", then
#   QT_VERSION=6.11.0 ./tools/build_android.sh. The script refuses a kit whose
#   template AGP differs from android/build.gradle, and clears the CMake cache and
#   build-android/android-build when the kit changes.
# ---------------------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build-android"

fail() { echo "ERROR: $1" >&2; exit 1; }

CONFIG="Debug"
LIB_ONLY=false
CONFIG_SET=false
for arg in "$@"; do
    case "${arg}" in
        Debug|Release)
            [[ "${CONFIG_SET}" == false ]] || fail "config given twice"
            CONFIG="${arg}"; CONFIG_SET=true ;;
        --lib-only) LIB_ONLY=true ;;
        *) fail "unknown argument '${arg}' (usage: $0 [Debug|Release] [--lib-only])" ;;
    esac
done

abi_to_kit() {
    case "$1" in
        arm64-v8a) echo android_arm64_v8a ;;
        armeabi-v7a) echo android_armv7 ;;
        x86) echo android_x86 ;;
        x86_64) echo android_x86_64 ;;
        *) return 1 ;;
    esac
}
kit_to_abi() {
    case "$1" in
        android_arm64_v8a) echo arm64-v8a ;;
        android_armv7) echo armeabi-v7a ;;
        android_x86) echo x86 ;;
        android_x86_64) echo x86_64 ;;
        *) return 1 ;;
    esac
}

# Qt version: the only place to bump it. The NDK defaults to the kit's ndk_version.
QT_VERSION="${QT_VERSION:-6.12.0}"
QT_ROOT="${QT_ROOT:-$HOME/Qt/${QT_VERSION}}"
CALAOS_ANDROID_ABIS="${CALAOS_ANDROID_ABIS:-arm64-v8a}"
FIRST_ABI="${CALAOS_ANDROID_ABIS%%;*}"
if [[ -z "${QT_ANDROID:-}" ]]; then
    kit="$(abi_to_kit "${FIRST_ABI}")" || fail "unknown ABI '${FIRST_ABI}' in CALAOS_ANDROID_ABIS"
    QT_ANDROID="${QT_ROOT}/${kit}"
fi
# The ABI of the configured kit drives the NDK toolchain (ANDROID_ABI).
ANDROID_ABI="$(kit_to_abi "$(basename "${QT_ANDROID}")")" \
    || fail "cannot derive the ABI from Qt kit name '$(basename "${QT_ANDROID}")'"
case ";${CALAOS_ANDROID_ABIS};" in
    *";${ANDROID_ABI};"*) ;;
    *) fail "Qt kit ABI '${ANDROID_ABI}' (${QT_ANDROID}) is not in CALAOS_ANDROID_ABIS='${CALAOS_ANDROID_ABIS}'" ;;
esac
QT_HOST="${QT_HOST:-${QT_ROOT}/macos}"

export ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-$HOME/Library/Android/sdk}"
# NDK version the Qt kit was built with ("ndk_version" in modules/Core.json).
QT_NDK_VERSION="$(sed -n 's/.*"ndk_version": *"\([^"]*\)".*/\1/p' "${QT_ANDROID}/modules/Core.json" 2>/dev/null | head -1)"
export ANDROID_NDK_ROOT="${ANDROID_NDK_ROOT:-${ANDROID_SDK_ROOT}/ndk/${QT_NDK_VERSION:-27.2.12479018}}"
if [[ -z "${JAVA_HOME:-}" ]] && command -v brew >/dev/null 2>&1; then
    JAVA_HOME="$(brew --prefix openjdk@17 2>/dev/null || true)/libexec/openjdk.jdk/Contents/Home"
fi
export JAVA_HOME="${JAVA_HOME:-}"
ANDROID_OPENSSL_DIR="${ANDROID_OPENSSL_DIR:-${ANDROID_SDK_ROOT}/android_openssl}"

# ninja: Homebrew, or the one shipped with the Qt installer (~/Qt/Tools/Ninja)
export PATH="$PATH:/opt/homebrew/bin:${QT_ROOT%/*}/Tools/Ninja"

[[ "${CONFIG}" == "Debug" || "${CONFIG}" == "Release" ]] || fail "config must be Debug or Release"
[[ -d "${QT_ANDROID}" ]] || fail "Qt Android kit not found: ${QT_ANDROID}"
[[ -d "${QT_HOST}" ]] || fail "Qt host (macOS) kit not found: ${QT_HOST}"
# Canonical paths (symlinks, trailing slashes) so the cache check below is stable.
QT_ANDROID="$(cd "${QT_ANDROID}" && pwd -P)"
QT_HOST="$(cd "${QT_HOST}" && pwd -P)"
[[ -d "${ANDROID_SDK_ROOT}" ]] || fail "Android SDK not found: ${ANDROID_SDK_ROOT}"
[[ -f "${ANDROID_NDK_ROOT}/build/cmake/android.toolchain.cmake" ]] || fail "Android NDK not found: ${ANDROID_NDK_ROOT}"
[[ -x "${JAVA_HOME}/bin/java" ]] || fail "JDK 17 not found (JAVA_HOME='${JAVA_HOME}'); brew install openjdk@17"
java_major="$("${JAVA_HOME}/bin/java" -version 2>&1 | sed -n '1s/.*version "\([0-9]*\).*/\1/p')"
[[ "${java_major}" == "17" ]] || fail "JDK 17 required, found major version '${java_major}' in JAVA_HOME='${JAVA_HOME}'"
[[ -f "${ANDROID_OPENSSL_DIR}/android_openssl.cmake" ]] || fail "KDAB android_openssl not found: ${ANDROID_OPENSSL_DIR}"
command -v ninja >/dev/null 2>&1 || fail "ninja not found in PATH"
command -v cmake >/dev/null 2>&1 || fail "cmake not found in PATH"

# android/build.gradle is ported from a Qt kit template; its AGP only works with the
# Gradle wrapper of that kit. Stop early rather than fail deep inside Gradle.
if [[ "${LIB_ONLY}" == false ]]; then
    agp_re='s/.*com\.android\.tools\.build:gradle:\([0-9.]*\).*/\1/p'
    our_agp="$(sed -n "${agp_re}" "${SCRIPT_DIR}/android/build.gradle" | head -1)"
    kit_agp="$(sed -n "${agp_re}" "${QT_ANDROID}/src/android/templates/build.gradle" 2>/dev/null | head -1)"
    if [[ -n "${kit_agp}" && "${our_agp}" != "${kit_agp}" ]]; then
        fail "android/build.gradle uses AGP ${our_agp} but the Qt kit template (${QT_ANDROID}) uses AGP ${kit_agp} with its own Gradle wrapper. Port that kit's templates into android/ (rollback to Qt 6.11: revert the commit 'build: port the Qt 6.12 Android templates (QT-1)')."
    fi
fi

# A cache configured against another Qt kit (version bump/rollback) keeps the old
# Qt6_DIR and toolchain paths, and androiddeployqt never removes files (Gradle
# wrapper, settings.gradle, Qt libs) copied from the previous kit: start clean.
if [[ -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
    cached_qt="$(sed -n 's/^Qt6_DIR:[A-Z]*=//p' "${BUILD_DIR}/CMakeCache.txt")"
    if [[ -n "${cached_qt}" && -d "${cached_qt}" ]]; then
        cached_qt="$(cd "${cached_qt}" && pwd -P)"
    fi
    if [[ -n "${cached_qt}" && "${cached_qt}" != "${QT_ANDROID}/"* ]]; then
        echo "==> Qt kit changed (cache: ${cached_qt}): clearing the CMake cache and android-build/"
        rm -rf "${BUILD_DIR}/CMakeCache.txt" "${BUILD_DIR}/CMakeFiles" "${BUILD_DIR}/android-build"
    fi
fi

echo "==> Configuring for Android (${CONFIG}, ABI: ${ANDROID_ABI}, ABIs: ${CALAOS_ANDROID_ABIS})"
echo "    Qt kit: ${QT_ANDROID}"
echo "    Qt host: ${QT_HOST}"
echo "    NDK: ${ANDROID_NDK_ROOT}"
cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE="${CONFIG}" \
    -DCMAKE_TOOLCHAIN_FILE="${ANDROID_NDK_ROOT}/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="${ANDROID_ABI}" \
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
ls -1 "${BUILD_DIR}"/libCalaosHome_*.so 2>/dev/null || true
if [[ "${LIB_ONLY}" == false ]]; then
    find "${BUILD_DIR}/android-build/build/outputs" -name '*.apk' 2>/dev/null
fi
