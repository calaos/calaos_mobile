#!/usr/bin/env bash
set -euo pipefail

# ---------------------------------------------------------------------------
# build_android.sh — Configure and build the Calaos Home Android app (Qt 6 + CMake + Ninja)
# Usage: ./build_android.sh [Debug|Release] [--aab] [--lib-only]
#   --lib-only  build only the native library (skip Gradle/APK packaging)
#   --aab       build the Android App Bundle (Play Store upload) instead of the APK
# ABIs: Debug builds arm64-v8a only (fast); Release builds the four ABIs
#   (arm64-v8a;armeabi-v7a;x86;x86_64) with the installed per-ABI Qt kits
#   (android_arm64_v8a is the main one, Qt configures the others as external
#   projects). Override with CALAOS_ANDROID_ABIS (semicolon list, main ABI first).
# Signing (Release): export CALAOS_KEYSTORE (path), CALAOS_KEYSTORE_PASS,
#   CALAOS_KEY_ALIAS and optionally CALAOS_KEY_PASS (defaults to the store
#   password). After the build the script signs a copy of the output with the upload
#   key (jarsigner for the AAB, apksigner for the APK), passing the passwords through
#   the environment only: they are never put on a command line nor echoed. Without
#   them the Release output is unsigned. (Signing through Gradle's signingConfig makes
#   androiddeployqt fail: it only knows the "-unsigned" APK name.)
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
AAB=false
CONFIG_SET=false
for arg in "$@"; do
    case "${arg}" in
        Debug|Release)
            [[ "${CONFIG_SET}" == false ]] || fail "config given twice"
            CONFIG="${arg}"; CONFIG_SET=true ;;
        --lib-only) LIB_ONLY=true ;;
        --aab) AAB=true ;;
        *) fail "unknown argument '${arg}' (usage: $0 [Debug|Release] [--aab] [--lib-only])" ;;
    esac
done
[[ "${AAB}" == false || "${LIB_ONLY}" == false ]] || fail "--aab and --lib-only are mutually exclusive"

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
if [[ "${CONFIG}" == "Release" ]]; then
    DEFAULT_ABIS="arm64-v8a;armeabi-v7a;x86;x86_64"
else
    DEFAULT_ABIS="arm64-v8a"
fi
CALAOS_ANDROID_ABIS="${CALAOS_ANDROID_ABIS:-${DEFAULT_ABIS}}"
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

# Every requested ABI needs its Qt kit. Qt finds the secondary kits next to the
# main one by itself; pass the paths explicitly so a custom QT_ROOT/QT_ANDROID works.
QT_ABI_ARGS=()
IFS=';' read -r -a REQUESTED_ABIS <<< "${CALAOS_ANDROID_ABIS}"
for abi in "${REQUESTED_ABIS[@]}"; do
    kit="$(abi_to_kit "${abi}")" || fail "unknown ABI '${abi}' in CALAOS_ANDROID_ABIS (valid: arm64-v8a armeabi-v7a x86 x86_64)"
    [[ -d "${QT_ROOT}/${kit}" ]] || fail "Qt kit for ABI '${abi}' not found: ${QT_ROOT}/${kit}"
    QT_ABI_ARGS+=("-DQT_PATH_ANDROID_ABI_${abi}=${QT_ROOT}/${kit}")
done

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

# Release signing: applied after the build. All or nothing,
# so a typo cannot silently produce an unsigned upload. Values are never printed.
SIGNED=false
if [[ "${CONFIG}" == "Release" ]]; then
    if [[ -n "${CALAOS_KEYSTORE:-}" || -n "${CALAOS_KEYSTORE_PASS:-}" || -n "${CALAOS_KEY_ALIAS:-}" ]]; then
        [[ -n "${CALAOS_KEYSTORE:-}" && -n "${CALAOS_KEYSTORE_PASS:-}" && -n "${CALAOS_KEY_ALIAS:-}" ]] \
            || fail "incomplete signing setup: CALAOS_KEYSTORE, CALAOS_KEYSTORE_PASS and CALAOS_KEY_ALIAS must all be set (CALAOS_KEY_PASS is optional)"
        [[ -f "${CALAOS_KEYSTORE}" ]] || fail "CALAOS_KEYSTORE does not point to a file"
        CALAOS_KEYSTORE="$(cd "$(dirname "${CALAOS_KEYSTORE}")" && pwd -P)/$(basename "${CALAOS_KEYSTORE}")"
        export CALAOS_KEYSTORE CALAOS_KEYSTORE_PASS CALAOS_KEY_ALIAS
        [[ -z "${CALAOS_KEY_PASS:-}" ]] || export CALAOS_KEY_PASS
        SIGNED=true
        [[ -x "${JAVA_HOME}/bin/jarsigner" ]] || fail "jarsigner not found in JAVA_HOME"
        APKSIGNER="$(ls -d "${ANDROID_SDK_ROOT}"/build-tools/*/apksigner 2>/dev/null | sort -V | tail -1)"
        [[ "${AAB}" == true || -x "${APKSIGNER}" ]] || fail "apksigner not found in ${ANDROID_SDK_ROOT}/build-tools"
    else
        echo "WARNING: CALAOS_KEYSTORE / CALAOS_KEYSTORE_PASS / CALAOS_KEY_ALIAS not set: the Release build will be UNSIGNED (not uploadable to the Play Store)." >&2
    fi
else
    # Debug is signed with the Android debug key; never leak release credentials into it.
    unset CALAOS_KEYSTORE CALAOS_KEYSTORE_PASS CALAOS_KEY_ALIAS CALAOS_KEY_PASS
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
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    "-DQT_ANDROID_MULTI_ABI_FORWARD_VARS=ANDROID_OPENSSL_DIR;CALAOS_ANDROID_ABIS;CMAKE_FIND_ROOT_PATH_MODE_PACKAGE;CMAKE_POLICY_VERSION_MINIMUM;ANDROID_PLATFORM" \
    "${QT_ABI_ARGS[@]}"

echo ""
echo "==> Building (${CONFIG}$([[ "${AAB}" == true ]] && echo ", AAB"))"
if [[ "${LIB_ONLY}" == true ]]; then
    cmake --build "${BUILD_DIR}" --target CalaosHome
elif [[ "${AAB}" == true ]]; then
    cmake --build "${BUILD_DIR}" --target aab
else
    cmake --build "${BUILD_DIR}"
fi

echo ""
if [[ "${LIB_ONLY}" == true ]]; then
    echo "==> Done. Native library:"
    ls -1 "${BUILD_DIR}"/libCalaosHome_*.so 2>/dev/null || true
    exit 0
fi

OUT_DIR="${BUILD_DIR}/android-build/build/outputs"
if [[ "${AAB}" == true ]]; then
    PKG_EXT=aab
    PKG_SRC="$(find "${OUT_DIR}/bundle/$(echo "${CONFIG}" | tr 'A-Z' 'a-z')" -name '*.aab' 2>/dev/null | head -1)"
else
    PKG_EXT=apk
    PKG_SRC="$(find "${OUT_DIR}/apk" -name '*.apk' -path "*$(echo "${CONFIG}" | tr 'A-Z' 'a-z')*" 2>/dev/null | head -1)"
fi
[[ -n "${PKG_SRC}" ]] || fail "no .${PKG_EXT} found under ${OUT_DIR}"

if [[ "${SIGNED}" == true ]]; then
    VERSION_NAME="$(head -1 "${SCRIPT_DIR}/VERSION.txt" | tr -d '[:space:]')"
    PKG_OUT="${BUILD_DIR}/CalaosHome-${VERSION_NAME}-release.${PKG_EXT}"
    echo "==> Signing ${PKG_EXT} with the upload key (alias ${CALAOS_KEY_ALIAS})"
    rm -f "${PKG_OUT}"
    cp "${PKG_SRC}" "${PKG_OUT}"
    if [[ "${AAB}" == true ]]; then
        # Gradle may leave a previously signed bundle in place when nothing changed.
        zip -q -d "${PKG_OUT}" 'META-INF/*.SF' 'META-INF/*.RSA' 'META-INF/*.DSA' 'META-INF/*.EC' >/dev/null 2>&1 || true
        keypass_args=()
        [[ -z "${CALAOS_KEY_PASS:-}" ]] || keypass_args=(-keypass:env CALAOS_KEY_PASS)
        "${JAVA_HOME}/bin/jarsigner" -keystore "${CALAOS_KEYSTORE}" \
            -storepass:env CALAOS_KEYSTORE_PASS ${keypass_args[@]+"${keypass_args[@]}"} \
            "${PKG_OUT}" "${CALAOS_KEY_ALIAS}" >/dev/null
        "${JAVA_HOME}/bin/jarsigner" -verify "${PKG_OUT}" | grep -m1 "jar verified" || fail "jarsigner -verify failed"
    else
        keypass_args=()
        [[ -z "${CALAOS_KEY_PASS:-}" ]] || keypass_args=(--key-pass env:CALAOS_KEY_PASS)
        PATH="${JAVA_HOME}/bin:${PATH}" "${APKSIGNER}" sign --ks "${CALAOS_KEYSTORE}" \
            --ks-key-alias "${CALAOS_KEY_ALIAS}" --ks-pass env:CALAOS_KEYSTORE_PASS \
            ${keypass_args[@]+"${keypass_args[@]}"} "${PKG_OUT}"
        PATH="${JAVA_HOME}/bin:${PATH}" "${APKSIGNER}" verify "${PKG_OUT}"
    fi
    echo "==> Done. Signed package:"
    echo "${PKG_OUT}"
else
    rm -f "${BUILD_DIR}"/CalaosHome-*-release.${PKG_EXT}   # stale signed copy of an older build
    echo "==> Done. Package ($([[ "${CONFIG}" == Release ]] && echo "UNSIGNED, not uploadable" || echo "debug key")):"
    echo "${PKG_SRC}"
fi
