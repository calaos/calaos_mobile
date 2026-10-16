---
name: qt6-android-build
description: How to configure, build, install and debug the Calaos Home Android app with Qt 6.12 + CMake on this Mac — toolchain version matrix, environment, build/deploy commands, logcat, and the packaging pitfalls (android/ overrides Qt templates, 16 KB pages). Use for any task that builds, packages or runs the Android app.
---

# Qt 6.12 Android build — Calaos Home

Keep this file accurate: if you discover a fact here is wrong, fix it in the same commit.

## Version matrix (source of truth: the installed Qt kit)

| Item | Value | Where it comes from |
|---|---|---|
| Qt | 6.12.0, `~/Qt/6.12.0/android_arm64_v8a` (+ `android_armv7`, `android_x86`, `android_x86_64`), host `~/Qt/6.12.0/macos`. 6.11.0 is kept, but an Android rollback is **not** knob-only: `android/build.gradle` uses AGP 9.2.1 (6.12 template), which needs Gradle >= 9.4.1 while the 6.11 kit ships Gradle 9.3.1. Revert the commit "build: port the Qt 6.12 Android templates (QT-1)", then `QT_VERSION=6.11.0 ./tools/build_android.sh` (the script refuses a kit whose template AGP differs from `android/build.gradle`). iOS rolls back with `QT_VERSION` alone | installed kits; `QT_VERSION` in `tools/build_android.sh` |
| NDK | **27.2.12479018** (same as 6.11) | `ndk_version` in `~/Qt/6.12.0/android_arm64_v8a/modules/Core.json`; the build script defaults `ANDROID_NDK_ROOT` to it |
| JDK | **17** (brew `openjdk@17`) | Qt template `JavaVersion.VERSION_17` (Qt docs list JDK 21 as tested; 17 builds fine with AGP 9.2) |
| AGP / Gradle | 9.2.1 / 9.5.1 (google-services plugin 4.5.0) | `~/Qt/6.12.0/android_arm64_v8a/src/android/templates/build.gradle`, `src/3rdparty/gradle/gradle/wrapper/gradle-wrapper.properties` |
| minSdk | **28** (Qt 6.12 floor, unchanged) | `QT_ANDROID_MIN_SDK_VERSION` in Qt cmake files |
| compile SDK | **37.0**, pinned with `QT_ANDROID_COMPILE_SDK_VERSION` in `cmake/Android.cmake` (configure fails with the sdkmanager command if `platforms;android-37.0` is missing). Write `37.0`, not `37`: from API 37 platforms carry a minor level | Qt 6.12 `QT_ANDROID_API_USED_FOR_JAVA android-37.0` in `lib/cmake/Qt6/QtPublicAndroidHelpers.cmake` |
| target SDK | **36** (ours, `cmake/Android.cmake`; Qt 6.12 defaults to 37 — raising it needs an Android 17 behaviour review) | Play Store requirement |
| OpenSSL | OpenSSL 3 from KDAB `android_openssl`, cloned at `~/Library/Android/sdk/android_openssl` | |

When in doubt, re-read the Qt template files above rather than trusting memory.

## Environment

```bash
export JAVA_HOME="$(brew --prefix openjdk@17)/libexec/openjdk.jdk/Contents/Home"
export ANDROID_SDK_ROOT="$HOME/Library/Android/sdk"
export ANDROID_NDK_ROOT="$ANDROID_SDK_ROOT/ndk/27.2.12479018"
export PATH="$ANDROID_SDK_ROOT/platform-tools:$PATH"   # adb
```

The canonical entry point is `tools/build_android.sh` (created by AND-1; mirrors `tools/build_ios.sh`). Prefer it over hand-written cmake lines, and keep it working.

`./tools/build_android.sh Debug --lib-only` builds only the native `libCalaosHome_arm64-v8a.so` (cmake target `CalaosHome`, skips Gradle). Ninja is taken from Homebrew or `~/Qt/Tools/Ninja`. `Qt6::GuiPrivate` is linked for `qpa/qplatformwindow.h`; the KDAB file is `android_openssl.cmake` (`add_android_openssl_libraries`), Debug uses its `no-asm` libs.

Debug builds target **arm64-v8a only** (fast). Release/AAB builds use all four ABIs.

## Build / deploy / debug

```bash
./tools/build_android.sh Debug                      # configure + build APK
APK=$(find build-android -name '*.apk' -path '*debug*' | head -1)
adb devices                                         # phone must show "device", not "unauthorized"
adb install -r "$APK"
adb shell am start -n fr.calaos.calaoshome/fr.calaos.calaoshome.HardwareUtils
adb logcat -c; adb logcat --pid="$(adb shell pidof -s fr.calaos.calaoshome)"   # app logs only
adb logcat -b crash                                 # native/Java crashes
```

When the Qt kit changes, the script itself deletes `build-android/CMakeCache.txt`, `CMakeFiles` and `android-build/`. If configure fails after an SDK/NDK change: delete them by hand (stale toolchain paths are cached).

## Packaging pitfalls

1. **`android/` overrides Qt's templates.** `QT_ANDROID_PACKAGE_SOURCE_DIR=android/` means every file there replaces Qt's generated one. Keep only files we really customize (manifest, `build.gradle`, Java sources, `res/` icons, `google-services.json`). Never ship a Gradle wrapper or a `res/values/libs.xml` there — Qt 6 generates its own and a stale copy breaks the build or library loading.
2. `androiddeployqt` writes `gradle.properties` values (`androidCompileSdkVersion`, `androidBuildToolsVersion`, `androidNdkVersion`, `qtMinSdkVersion`, `qtTargetSdkVersion`, `androidPackageName`, `qtAndroidDir`, …). `build.gradle` must consume those, as the Qt template does.
3. The manifest must not carry a `package=` attribute (AGP 8+: `namespace` in Gradle). Keep the `-- %%INSERT_...%% --` placeholders Qt expects.
4. **16 KB page size** (Play Store requirement for Android 15+ devices): check every `.so` with
   `"$ANDROID_NDK_ROOT"/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-readelf -lW lib.so | grep LOAD` → alignment `0x4000`,
   and the APK with `"$ANDROID_SDK_ROOT"/build-tools/36.0.0/zipalign -c -P 16 -v 4 app.apk`.
5. CMake: anything Apple-only (OBJC/OBJCXX languages, frameworks) must be guarded by `if(IOS)` — the iOS build (`tools/build_ios.sh Debug`) must keep working after any change to `CMakeLists.txt` or `cmake/`.
6. No `settings.gradle`, `gradle.properties` or `project.properties` in `android/`: androiddeployqt copies Qt's own `gradle.properties`, wrapper and (Qt 6.12+) the template `settings.gradle` that only scopes the Gradle root project. The debug APK lands in `build-android/android-build/build/outputs/apk/debug/android-build-debug.apk`. After deleting files from `android/`, also `rm -rf build-android/android-build` — androiddeployqt never removes stale copies (the script does it automatically when the Qt kit changes, since the Gradle wrapper, `settings.gradle` and Qt libs differ per kit).
7. XML comments in `AndroidManifest.xml` must not contain `--` (e.g. quoting the `-- %%INSERT_...%% --` placeholders): androiddeployqt fails with `Expected '>', but got ' '`.
8. Test phone: a Play Store build of `fr.calaos.calaoshome` (release-signed) makes `adb install -r` of a debug APK fail with `INSTALL_FAILED_UPDATE_INCOMPATIBLE`; uninstalling it deletes that app's data, so ask the user first. MIUI asks for an on-screen confirmation of each USB install of a new package (`INSTALL_FAILED_USER_RESTRICTED: Install canceled by user` when nobody taps it); "Install via USB" must be enabled in Developer options.
9. Since Qt 6.12 the template's `useLegacyPackaging = legacyPackaging.toBoolean()` really honours `legacyPackaging=false`: native libs are stored uncompressed and page-aligned in the APK and loaded from it (not extracted), so a debug APK is ~2.5x bigger than with 6.11 (whose template turned the string "false" into true). Store downloads are compressed by Play anyway; `zipalign -c -P 16` must still pass.
10. Back key: the manifest sets `android:enableOnBackInvokedCallback="true"` (Qt 6.12 template). Qt registers an `OnBackInvokedCallback` and delivers back as `Qt::Key_Back`; if QML does not accept it, Qt calls `moveTaskToBack()` instead of finishing the activity.
11. Network changes: `HardwareUtilsAndroid` loads Qt's `QNetworkInformation` Android backend (plugin `libplugins_networkinformation_qandroidnetworkinformation_*.so`, deployed automatically because the app links Qt6::Network; `ACCESS_NETWORK_STATE` comes from the Qt Network module manifest merge) and emits `networkStatusChanged` on reachability/transport-medium changes. Android also reports `Reachability::Unknown` during handovers; `getNetworkStatus()` then falls back to the Java ConnectivityManager query. Emulator note: the emulated network can flap by itself under heavy host load.
