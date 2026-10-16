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

`./tools/build_android.sh Debug --lib-only` builds only the native `libCalaosHome_arm64-v8a.so` (cmake target `CalaosHome`, skips Gradle). Ninja is taken from Homebrew or `~/Qt/Tools/Ninja`. The KDAB file is `android_openssl.cmake` (`add_android_openssl_libraries`), Debug uses its `no-asm` libs.

Debug builds target **arm64-v8a only** (fast). `Release` builds all four ABIs (`arm64-v8a;armeabi-v7a;x86;x86_64`, override with `CALAOS_ANDROID_ABIS`, main ABI first): the main ABI's kit is the one CMake is configured with, Qt configures the other three as external projects with their own kits (`QT_ANDROID_ABIS` target property from `cmake/Android.cmake`, `QT_PATH_ANDROID_ABI_<abi>` and `QT_ANDROID_MULTI_ABI_FORWARD_VARS` passed by the script). Any single ABI also configures and builds alone (the old "x86_64-only CMake error" was not reproducible with Qt 6.12; tested x86, x86_64, armeabi-v7a).

Release / Play Store:

```bash
export CALAOS_KEYSTORE=/path/upload.jks CALAOS_KEY_ALIAS=upload
read -rs CALAOS_KEYSTORE_PASS; export CALAOS_KEYSTORE_PASS      # CALAOS_KEY_PASS optional (defaults to it)
./tools/build_android.sh Release --aab    # -> build-android/CalaosHome-<ver>-release.aab (signed copy)
```

Without the `CALAOS_*` variables the AAB stays unsigned (loud warning, file under `android-build/build/outputs/bundle/release/`); a partial set is an error. For testing use a throwaway keystore in `/tmp` (`keytool -genkeypair -storetype PKCS12 ...`, then delete it); with a PKCS12 keystore the key password is the store password (a different `-keypass` silently breaks signing).

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
10. Back key: the manifest sets `android:enableOnBackInvokedCallback="true"` (Qt 6.12 template). Qt registers an `OnBackInvokedCallback` and delivers back as `Qt::Key_Back`; if QML does not accept the **release**, Qt calls `moveTaskToBack()` instead of finishing the activity (`qml/mobile/main.qml` leaves it unaccepted at the root on purpose). The gesture and the navigation-bar button arrive once (press, release), but on Android 13+ a back that comes in as a key event (`adb shell input keyevent KEYCODE_BACK`, hardware/keyboard back) arrives twice: press, press, release, release (key path + callback). Test both `input keyevent` and an edge swipe (`adb shell input swipe 2 1200 600 1200 300` in gesture mode); 3-button mode: `adb shell cmd overlay enable com.android.internal.systemui.navbar.threebutton` (`...navbar.gestural` to go back; switching kills cached app processes).
11. Network changes: `HardwareUtilsAndroid` loads Qt's `QNetworkInformation` Android backend (plugin `libplugins_networkinformation_qandroidnetworkinformation_*.so`, deployed automatically because the app links Qt6::Network; `ACCESS_NETWORK_STATE` comes from the Qt Network module manifest merge) and emits `networkStatusChanged` on reachability/transport-medium changes. Android also reports `Reachability::Unknown` during handovers; `getNetworkStatus()` then falls back to the Java ConnectivityManager query. Emulator note: the emulated network can flap by itself under heavy host load.
12. **Release signing is done by the script after the build, not by Gradle or `androiddeployqt --sign`.** A Gradle `signingConfig` renames the APK to `android-build-release.apk`, and androiddeployqt (which only knows `-unsigned`/`-signed`) then exits 20 without any message, failing even `--aab`; it would also hand the passwords to the long-lived Gradle daemon. `QT_ANDROID_SIGN_*` works (androiddeployqt 6.12 does run apksigner) but passes the passwords on the signer command lines. The script copies the output and runs `jarsigner -storepass:env` (AAB) / `apksigner --ks-pass env:` (APK) with the passwords set one-off in the signer's own environment only (`export -n` for the rest of the build). Before the build it validates store, alias and key password with `keytool`; for a PKCS12 store (single password; keytool ignores `-keypass` there) a `CALAOS_KEY_PASS` different from `CALAOS_KEYSTORE_PASS` is rejected explicitly, only a JKS store can have distinct passwords.
13. **`WRITE_EXTERNAL_STORAGE`** is inserted by Qt Core's `QT_ANDROID_PERMISSIONS` into the main manifest (same file as any `tools:node="remove"` would be, which conflicts), so it is removed by the overlay `android/release/AndroidManifest.xml` (`sourceSets.release` in `build.gradle`): release only, debug keeps it. Verify: `aapt2 dump permissions app.apk`.
14. **R8/minify is deliberately off** (`minifyEnabled false`): Qt's Java (`org.qtproject.qt.android.*`) is resolved by name from native code with no consumer keep rules, the app's own Java is small, the win is a few MB of dex. If you enable it, keep all JNI-by-name targets: `HardwareUtils` static helpers (`showAlertMessage`, `getNetworkStatus`, `inputtextDialog`, `setNativesReady`, `getStoredPushToken`, `requestPushToken`, `takePendingEventUuid`, `deliverPushToken`), `HardwareUtilsNatives` (`registerNativeMethods`), `CredentialStore` methods called from C++, plus the manifest components `CalaosMessagingService`, `PushPictureWorker`, and verify a release APK on a device.
15. **16 KB pages**: 64-bit libs (arm64-v8a, x86_64) have LOAD align `0x4000`; the 32-bit ones (armeabi-v7a, x86) are `0x1000`, which is fine: 16 KB devices are 64-bit only and Play's requirement targets 64-bit native code. `zipalign -c -P 16 4` passes on the universal APK (bundletool aligns uncompressed `.so`).
16. Handy tools: bundletool is not installed; `gh release download 1.18.3 --repo google/bundletool -p bundletool-all-1.18.3.jar` into `/tmp`, then `java -jar ... dump manifest --bundle x.aab`, `build-apks --mode=universal --ks=...` for an installable APK, or `build-apks` + `extract-apks --device-spec=spec.json` (`{"supportedAbis":["x86_64"],"supportedLocales":["en"],"screenDensity":420,"sdkVersion":36}`) for the per-ABI splits, and `get-size total --apks=x.apks --dimensions=ABI` for download sizes. Signing the test APKs with `~/.android/debug.keystore` (alias `androiddebugkey`, password `android`) lets them replace a debug build with `adb install-multiple -r` without losing app data. A release AAB is ~127 MB (uncompressed Qt libs, 67 `.so` per ABI, 4 ABIs); Play delivers per-ABI splits (~31 MB download for arm64-v8a).
17. **Deployed Qt plugins and QML modules** (AND-9): `cmake/Android.cmake` drops the plugins the app never loads with `qt_import_plugins(... EXCLUDE_BY_TYPE / EXCLUDE ...)` (qmltooling in non-Debug builds, virtual keyboard input context, icon engines, offscreen platform, icns/ico/tga/tiff/wbmp); kept: Android platform, OpenSSL TLS, network information, jpeg/gif/svg/webp. `calaos_android_limit_qml_scan()` (called after `qt_finalize_executable()`) restricts androiddeployqt's QML import scan to `qml/mobile`, `qml/SharedComponents`, `qml/quickflux` (same list as `cmake/qml_imports` for iOS): Qt always adds the target source dir (repo root), whose desktop QML and tests pulled VirtualKeyboard, QtTest, Dialogs, FolderListModel in. Check with `android-deploy-plugins` / `qml-root-path` in `build-android/android-CalaosHome-deployment-settings.json`. The four unused Quick Controls styles (Fusion, Imagine, Universal, FluentWinUI3, ~11 MB per ABI) stay: `import QtQuick.Controls` imports them all and they are listed in `libs.xml` (loaded at startup), so a Gradle `jniLibs.excludes` would crash the app. After changing what is deployed, `rm -rf build-android/android-build` (androiddeployqt never removes stale libs).
