---
name: qt6-android-build
description: How to configure, build, install and debug the Calaos Home Android app with Qt 6.11 + CMake on this Mac — toolchain version matrix, environment, build/deploy commands, logcat, and the packaging pitfalls (android/ overrides Qt templates, 16 KB pages). Use for any task that builds, packages or runs the Android app.
---

# Qt 6.11 Android build — Calaos Home

Keep this file accurate: if you discover a fact here is wrong, fix it in the same commit.

## Version matrix (source of truth: the installed Qt kit)

| Item | Value | Where it comes from |
|---|---|---|
| Qt | 6.11.0, `~/Qt/6.11.0/android_arm64_v8a` (+ `android_armv7`, `android_x86`, `android_x86_64`), host `~/Qt/6.11.0/macos` | installed kits |
| NDK | **27.2.12479018** | `ndk_version` in `~/Qt/6.11.0/android_arm64_v8a/modules/*.json` |
| JDK | **17** (brew `openjdk@17`) | Qt template `JavaVersion.VERSION_17` |
| AGP / Gradle | 9.0.0 / 9.3.1 | `~/Qt/6.11.0/android_arm64_v8a/src/android/templates/build.gradle`, `src/3rdparty/gradle/gradle/wrapper/gradle-wrapper.properties` |
| minSdk | **28** (Qt 6.11 floor) | `QT_ANDROID_MIN_SDK_VERSION` in Qt cmake files |
| compile/target SDK | **36** | `QT_ANDROID_API_USED_FOR_JAVA android-36`; Play Store requirement |
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

If configure fails after an SDK/NDK/Qt change: delete `build-android/CMakeCache.txt` and `build-android/CMakeFiles` (stale toolchain paths are cached).

## Packaging pitfalls

1. **`android/` overrides Qt's templates.** `QT_ANDROID_PACKAGE_SOURCE_DIR=android/` means every file there replaces Qt's generated one. Keep only files we really customize (manifest, `build.gradle`, Java sources, `res/` icons, `google-services.json`). Never ship a Gradle wrapper or a `res/values/libs.xml` there — Qt 6 generates its own and a stale copy breaks the build or library loading.
2. `androiddeployqt` writes `gradle.properties` values (`androidCompileSdkVersion`, `androidBuildToolsVersion`, `androidNdkVersion`, `qtMinSdkVersion`, `qtTargetSdkVersion`, `androidPackageName`, `qtAndroidDir`, …). `build.gradle` must consume those, as the Qt template does.
3. The manifest must not carry a `package=` attribute (AGP 8+: `namespace` in Gradle). Keep the `-- %%INSERT_...%% --` placeholders Qt expects.
4. **16 KB page size** (Play Store requirement for Android 15+ devices): check every `.so` with
   `"$ANDROID_NDK_ROOT"/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-readelf -lW lib.so | grep LOAD` → alignment `0x4000`,
   and the APK with `"$ANDROID_SDK_ROOT"/build-tools/36.0.0/zipalign -c -P 16 -v 4 app.apk`.
5. CMake: anything Apple-only (OBJC/OBJCXX languages, frameworks) must be guarded by `if(IOS)` — the iOS build (`tools/build_ios.sh Debug`) must keep working after any change to `CMakeLists.txt` or `cmake/`.
