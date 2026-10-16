# Calaos Home — CMake migration plan (mobile only)

> Migration target: **iOS + Android build via CMake (Qt 6)**.
> **Desktop continues to use qmake** (`desktop.pro`) until further notice.
> This document is the authoritative TODO for the agent that will implement
> the migration. Read it top‑to‑bottom; check items off as you go.
>
> **Status (Android): done.** Android is built with CMake only and qmake no
> longer supports it. §4 below describes what was implemented; the day-to-day
> reference is [android.md](android.md) and `tools/build_android.sh`. The
> original Android plan text (native Firebase SDK, Qt 6.10, API 26/34) is
> superseded where it differs. The iOS parts are unchanged.

---

## 0. Goals & non‑goals

### Goals
- Single command to build iOS: `cmake --build build-ios --target archive`.
- Single command to build Android APK/AAB: `cmake --build build-android --target apk`.
- Eliminate every manual Xcode step listed in [doc/ios.md §3](ios.md) by
  declaring capabilities, App Groups, Push entitlements, the `PushNotif`
  notification‑service extension, version numbers and signing in CMake.
- GitHub Actions workflows that produce signed TestFlight + Play Store
  internal‑track artifacts.
- Zero functional regression: same bundle id, same package id, same Firebase
  config, same keychain / app group, same Qt modules, same QML/QRC content.

### Non‑goals (this iteration)
- **Do not** touch `desktop.pro` / `calaos.pri` for the desktop variant. It
  must keep building exactly as today.
- **Do not** vendor new third‑party deps. Reuse the ones already in `3rd_party/`.
- **Do not** rewrite C++/Obj‑C/Java sources. Only the build system changes.
- **Do not** modify [3rd_party/](../3rd_party) sources.

---

## 1. Strategy overview

### 1.1 Two build systems coexisting

```
calaos.pri    + desktop.pro    → qmake build (desktop, unchanged)
calaos.pri    + mobile.pro     → qmake build (desktop test fallback and legacy iOS; Android is an error())
CMakeLists.txt (new, root)     → CMake build (mobile = iOS + Android; the only Android build)
```

The new top‑level `CMakeLists.txt` will guard itself with
`if(IOS OR ANDROID)` and **error out otherwise**, so a desktop developer who
runs `cmake .` by mistake gets a clear message redirecting them to qmake.

`mobile.pro` and `ios/ios.pri` are kept for a transition period (one or two
releases) and then deleted. `android/android.pri` is already deleted and the
`android` branch of `mobile.pro` is an `error()` pointing to
`tools/build_android.sh`.

### 1.2 Source‑of‑truth re‑use

The C++ source list currently lives in [calaos.pri](../calaos.pri). Do **not**
duplicate it. Either:

- **Option 1 (recommended)**: parse `calaos.pri` from CMake using a small
  helper (`file(STRINGS ...)` + regex on `SOURCES +=` / `HEADERS +=`).
- **Option 2**: extract the shared source list into a new
  `cmake/SharedSources.cmake` and have `calaos.pri` `include()` a generated
  qmake‑syntax twin produced by CMake at configure time.

Pick Option 1 for the first cut; it keeps qmake authoritative until desktop
also migrates.

### 1.3 Qt 6 CMake APIs to use

- `qt_standard_project_setup()` — sane defaults (AUTOMOC, RCC integration).
- `qt_add_executable(CalaosHome ...)` — main app, `MACOSX_BUNDLE` on iOS.
- `qt_add_resources(CalaosHome ...)` — replaces `RESOURCES += *.qrc`.
- `qt_add_translations(CalaosHome TS_FILES ...)` — replaces `lang/lang.pri`.
- `qt_add_executable(PushNotif ...)` for the iOS notification service
  extension; embed via `XCODE_EMBED_APP_EXTENSIONS`.
- `qt_add_android_apk_target()` / `qt_finalize_executable()` — produces APK,
  signs it, and assembles the AAB (replaces `make apk` / `make aab`).
- `set_target_properties(... XCODE_ATTRIBUTE_*)` for every iOS build
  setting currently faked through `QMAKE_MAC_XCODE_SETTINGS`.

Min Qt for this migration: **Qt 6.6** (full `qt_add_translations`,
`qt_finalize_target`, mature Xcode codesign attributes). The repo already
ships `Qt_6_10_0_for_iOS-*` build dirs, so this is fine.

---

## 2. Target directory layout

```
CMakeLists.txt                   # NEW — top level, mobile only
cmake/
    CalaosHomeSources.cmake      # NEW — shared C++/HEADER list (parsed from calaos.pri or duplicated)
    CalaosHomeQrc.cmake          # NEW — RESOURCES list
    iOS.cmake                    # NEW — iOS specifics (Xcode attrs, capabilities, appex)
    Android.cmake                # NEW — Android specifics (OpenSSL, SDK levels, ABIs, deployment trimming)
    Versioning.cmake             # NEW — read VERSION file, write to plist + manifest
ios/
    CMakeLists.txt               # NEW — defines the PushNotif appex target
    PushNotif/                   # unchanged
android/                         # manifest, build.gradle, Java glue, JNI bridge (no own CMakeLists.txt)
3rd_party/
    quickflux/CMakeLists.txt     # NEW — wraps quickflux.pri (or include the upstream cmake)
    qt-qml-models/CMakeLists.txt # NEW — three .cpp/.h, trivial
VERSION                          # NEW — single source of truth, e.g. "3.7.2"
```

The .qrc files **stay where they are**; CMake just references them.

---

## 3. iOS migration — detail

### 3.1 What we get for free

Qt 6 + Xcode generator handles automatically:
- Bundle creation, `Info.plist` template, asset catalogs,
- Static linking of QtCore/Quick/Network/WebSockets,
- `qmlimportscanner` deployment,
- Signing with auto‑managed profile when `XCODE_ATTRIBUTE_CODE_SIGN_STYLE`
  is `Automatic`.

### 3.2 What we must declare explicitly

Translation table from current manual steps → CMake:

| Manual step today | CMake replacement |
|---|---|
| Capabilities checkboxes (Push, Keychain, App Groups) | `XCODE_ATTRIBUTE_SystemCapabilities` is read‑only; instead, **commit a complete `.entitlements`** (already done — see [ios/pushnotifications.entitlements](../ios/pushnotifications.entitlements)) and set `XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS`. The provisioning profile carries the capabilities. |
| Create `PushNotif` Notification Service Extension target | Second `qt_add_executable(PushNotif ...)` with `MACOSX_BUNDLE_INFO_PLIST = ios/PushNotif/Info.plist` and bundle id `fr.calaos.CalaosMobile.PushNotif`. |
| Symlink `ios/PushNotif/` into the build dir | Not needed — CMake references sources by absolute path. |
| Strip Qt linker flags from PushNotif | `target_link_libraries(PushNotif PRIVATE Foundation UserNotifications)` only — never link any `Qt::*`. |
| Add `A0SimpleKeychain.m`, `A0SimpleKeychain+KeyPair.m` to PushNotif compile sources | List them in PushNotif's `target_sources()`. |
| `MARKETING_VERSION` / `CURRENT_PROJECT_VERSION` | `cmake/Versioning.cmake` reads the `VERSION` file and sets `XCODE_ATTRIBUTE_MARKETING_VERSION` / `XCODE_ATTRIBUTE_CURRENT_PROJECT_VERSION` on both targets. |
| Embed `PushNotif.appex` into main app | `set_target_properties(CalaosHome PROPERTIES XCODE_EMBED_APP_EXTENSIONS PushNotif)`. |
| Select team / signing | `XCODE_ATTRIBUTE_DEVELOPMENT_TEAM = 9WYC46992U`, `XCODE_ATTRIBUTE_CODE_SIGN_STYLE = Automatic` (or `Manual` + `PROVISIONING_PROFILE_SPECIFIER` for CI). |
| Disable bitcode | `XCODE_ATTRIBUTE_ENABLE_BITCODE = NO`. |
| Archive + export | Custom target `archive` running `xcodebuild archive` + `-exportArchive` with `ios/ExportOptions.plist`. |

### 3.3 Single‑command build

```bash
cmake -S . -B build-ios -G Xcode \
      -DCMAKE_SYSTEM_NAME=iOS \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
      -DCMAKE_PREFIX_PATH=/path/to/Qt/6.10.0/ios \
      -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=9WYC46992U
cmake --build build-ios --config Release --target archive
```

`archive` is a custom target that runs `xcodebuild archive` then
`xcodebuild -exportArchive` against `ios/ExportOptions.plist` (also new).

### 3.4 Files to create

- `ios/ExportOptions.plist` (app‑store, automatic signing, team
  `9WYC46992U`, `stripSwiftSymbols=true`).
- `ios/PushNotif/PushNotif.entitlements` already exists per the doc; verify
  before relying on it. If absent, generate it (keychain group +
  app‑group only).
- `ios/CMakeLists.txt` for the appex target.

---

## 4. Android migration — detail (implemented)

Everything below is in place; see [android.md](android.md) for the full
build / install / release flow.

- **Strategy**: `qt_add_executable(CalaosHome MANUAL_FINALIZATION ...)` +
  `qt_finalize_executable(CalaosHome)` in the root [CMakeLists.txt](../CMakeLists.txt);
  Qt generates the `androiddeployqt` JSON and drives Gradle.
- **Target properties** ([cmake/Android.cmake](../cmake/Android.cmake)):
  `QT_ANDROID_PACKAGE_SOURCE_DIR` = `android/`, version name/code from
  `VERSION.txt` ([cmake/Versioning.cmake](../cmake/Versioning.cmake)),
  min SDK 28, target SDK 36, compile SDK 37.0, `QT_ANDROID_ABIS` from
  `CALAOS_ANDROID_ABIS` (arm64-v8a in Debug, four ABIs in Release).
- **Push (Firebase)**: Firebase Java SDK in [android/build.gradle](../android/build.gradle)
  and Java sources; no native Firebase SDK and no CMake linkage (android.md §6).
- **OpenSSL**: KDAB `android_openssl` (`android_openssl.cmake`,
  `add_android_openssl_libraries(CalaosHome)`), root in `ANDROID_OPENSSL_DIR`
  (default `$ANDROID_SDK_ROOT/android_openssl`).
- **Gradle / manifest**: [android/AndroidManifest.xml](../android/AndroidManifest.xml)
  and [android/build.gradle](../android/build.gradle) are ported from the Qt 6.12
  templates; no Gradle wrapper, `gradle.properties` or `libs.xml` in `android/`
  (Qt generates them). `versionName` / `versionCode` are injected by Qt.
- **Single-command build**: `./tools/build_android.sh [Debug|Release] [--aab] [--lib-only]`
  wraps the `cmake` configure (NDK toolchain, `ANDROID_PLATFORM=android-28`,
  Qt kits, `QT_HOST_PATH`, per-ABI kits) and build, and signs Release output
  from `CALAOS_*` environment variables. Use it rather than hand-written
  `cmake` lines.

---

## 5. Translations

Replace [lang/lang.pri](../lang/lang.pri) for the mobile build with:

```cmake
qt_add_translations(CalaosHome
    TS_FILES lang/calaos_fr.ts lang/calaos_de.ts
    LUPDATE_OPTIONS -no-obsolete)
```

Keep the Go auto‑translate tool ([lang/auto_translate.go](../lang/auto_translate.go))
unchanged — it operates on the `.ts` files directly.

---

## 6. CI/CD plan (Phase 4)

After local builds work, add two workflows under
[.github/workflows/](../.github/workflows/):

### 6.1 `ios-release.yml`
- Trigger: tags `v*` + `workflow_dispatch`.
- Runner: `macos-15`.
- Steps: install Qt via `jurplel/install-qt-action` (target `ios`),
  install Ruby + Fastlane, run `bundle exec fastlane beta`.
- Secrets: `MATCH_PASSWORD`, `MATCH_GIT_BASIC_AUTHORIZATION`,
  `APP_STORE_CONNECT_API_KEY_KEY/_KEY_ID/_ISSUER_ID`.
- Artifact: `.ipa` to App Store Connect (TestFlight) via `pilot`.

### 6.2 `android-release.yml`
- Trigger: same.
- Runner: `ubuntu-24.04`.
- Steps: install Qt (target `android`), Android SDK/NDK, KDAB android_openssl
  (cached), then `./tools/build_android.sh Release --aab` (signing from
  `CALAOS_*`), then upload via `r0adkll/upload-google-play`. The script is
  written for macOS: on `ubuntu-24.04` set `QT_HOST` to the Linux host kit
  (`$QT_ROOT/gcc_64`), `ANDROID_SDK_ROOT` and `JAVA_HOME` (JDK 17), and map
  the CI secrets to the `CALAOS_KEYSTORE` (decode `KEYSTORE_BASE64` to a file),
  `CALAOS_KEYSTORE_PASS`, `CALAOS_KEY_ALIAS`, `CALAOS_KEY_PASS` variables.
- Secrets: `KEYSTORE_BASE64`, `KEYSTORE_PASSWORD`, `KEY_ALIAS`,
  `KEY_PASSWORD`, `PLAY_SERVICE_ACCOUNT_JSON`.

Fastlane is **only** used for iOS (provisioning sanity); Android stays in
plain GitHub Actions because its signing model is simple enough.

---

## 7. Implementation TODO (do these in order)

> Each top‑level item should be a separate commit with a clear message.
> Mark `[x]` as you finish; PR can be merged once Phase 1 + Phase 2 are green.

### Phase 0 — Preparation
- [x] Add `VERSION` file at the repo root containing `3.7.2` (or current
      version pulled from `AndroidManifest.xml` / `AppInfo.plist`).
- [x] Add `cmake/Versioning.cmake` reading `VERSION` and exposing
      `CALAOS_VERSION` (semver) and `CALAOS_VERSION_CODE` (integer derived,
      e.g. `MAJOR*10000 + MINOR*100 + PATCH`).
- [x] Add `.gitignore` entries for `build-ios/`, `build-android/`,
      `CMakeFiles/`, `CMakeCache.txt`.
- [x] Document the new commands in [ios.md](ios.md) and
      [android.md](android.md). (android.md now documents the CMake build
      only: the qmake Android instructions were removed in AND-6.)

### Phase 1 — Skeleton CMake (no platform yet)
- [x] Create top‑level `CMakeLists.txt`:
  - `cmake_minimum_required(VERSION 3.21)`,
  - `project(CalaosHome LANGUAGES CXX OBJC OBJCXX)`,
  - `if(NOT (IOS OR ANDROID)) message(FATAL_ERROR "Use qmake for desktop") endif()`,
  - `find_package(Qt6 6.6 REQUIRED COMPONENTS Core Qml Quick Network Gui WebSockets Svg LinguistTools)`,
  - `qt_standard_project_setup(REQUIRES 6.6)`.
- [x] Create `cmake/CalaosHomeSources.cmake` with the C++/HEADERS list copied
      from [calaos.pri](../calaos.pri). Add a brief comment that this is the
      mobile counterpart and must be kept in sync with `calaos.pri` until
      desktop also migrates. (A regression test: `tools/check_sources.sh`
      diffs the two lists at CI time.)
- [x] Add `qt_add_executable(CalaosHome ...)` with the shared sources.
- [x] Add `qt_add_resources` for `qml_mobile.qrc`, `img_mobile.qrc`,
      `fonts_mobile.qrc`, `qml_shared.qrc`, `lang.qrc`.
- [x] Add `qt_add_translations` for `lang/calaos_*.ts`.
- [x] Wrap [3rd_party/quickflux/quickflux.pri](../3rd_party/quickflux/quickflux.pri)
      and [3rd_party/qt-qml-models/QtQmlModels.pri](../3rd_party/qt-qml-models/QtQmlModels.pri)
      with thin `CMakeLists.txt` files (or use `add_library(... INTERFACE)`
      with hand‑listed sources). Quickflux upstream has CMake support — prefer
      that.
- [x] Add `target_compile_definitions(CalaosHome PRIVATE CALAOS_MOBILE)`.

### Phase 2 — Android working build
- [x] Create `cmake/Android.cmake`, included from root when `ANDROID`.
- [x] Push through the Firebase Java SDK (Gradle), no native SDK to link
      (§4; superseded the original native-SDK plan).
- [x] Implement android_openssl include (§4). Provide `ANDROID_OPENSSL_DIR`
      cache var; default to `$ANDROID_SDK_ROOT/android_openssl`.
- [x] Add `android/HardwareUtils_Android.cpp` and `.h` to the target sources
      via `target_sources(CalaosHome PRIVATE ...)` when `ANDROID`.
- [x] Set `QT_ANDROID_PACKAGE_SOURCE_DIR`, `QT_ANDROID_VERSION_NAME`,
      `QT_ANDROID_VERSION_CODE`, `QT_ANDROID_MIN_SDK_VERSION = 28`,
      `QT_ANDROID_TARGET_SDK_VERSION = 36`, `QT_ANDROID_ABIS`.
- [x] Set `QT_ANDROID_EXTRA_LIBS` to the OpenSSL `.so`s for every ABI being
      built (KDAB's `openssl.cmake` exposes a helper).
- [x] Remove `versionName` / `versionCode` literals from
      [android/AndroidManifest.xml](../android/AndroidManifest.xml) — they are
      now injected by Qt at deploy time.
- [x] Verify Java glue is picked up via `QT_ANDROID_PACKAGE_SOURCE_DIR =
      ${CMAKE_SOURCE_DIR}/android`.
- [x] Build, install on a device, verify FCM token is received and a test
      push is delivered (phone tests recorded in `docs/BOARD.md`).
- [x] ~~Regression test: compare the qmake-built APK and the CMake-built APK~~
      Obsolete: the qmake Android build no longer exists.

### Phase 3 — iOS working build
- [x] Create `cmake/iOS.cmake`, included from root when `IOS`.
- [x] Add main target Xcode attributes (`PRODUCT_BUNDLE_IDENTIFIER`,
      `DEVELOPMENT_TEAM`, `CODE_SIGN_ENTITLEMENTS`, `ENABLE_BITCODE = NO`,
      `MARKETING_VERSION`, `CURRENT_PROJECT_VERSION`).
- [x] Set `MACOSX_BUNDLE_INFO_PLIST = ios/AppInfo.plist`.
- [x] Add Obj‑C++ sources from [ios/ios.pri](../ios/ios.pri):
      `HardwareUtils.mm`, `Reachability.m`, `AlertPrompt.m`,
      `SimpleKeychain/A0SimpleKeychain.m`, `SimpleKeychain/A0SimpleKeychain+KeyPair.m`.
- [x] Link `SystemConfiguration` and `UserNotifications` frameworks.
- [x] Add `QMAKE_ASSET_CATALOGS` equivalent: list
      `ios/ressources.xcassets` in `target_sources` and Xcode picks it up.
- [x] Register `ios/Splash.storyboard` (`set_source_files_properties` with
      `MACOSX_PACKAGE_LOCATION=Resources`).
- [x] Create `ios/CMakeLists.txt` with the `PushNotif` target:
  - `qt_add_executable(PushNotif MACOSX_BUNDLE)`, but **without** linking any
    `Qt::*` library.
  - Sources: `ios/PushNotif/NotificationService.m`,
    `ios/SimpleKeychain/A0SimpleKeychain.m`,
    `ios/SimpleKeychain/A0SimpleKeychain+KeyPair.m`.
  - Frameworks: `UserNotifications`, `Foundation`.
  - Bundle id: `fr.calaos.CalaosMobile.PushNotif`.
  - `XCODE_PRODUCT_TYPE = com.apple.product-type.app-extension`.
  - `XCODE_ATTRIBUTE_INFOPLIST_FILE = ios/PushNotif/Info.plist`.
  - `XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS = ios/PushNotif/PushNotif.entitlements`.
- [x] On the main `CalaosHome` target:
      `set_target_properties(CalaosHome PROPERTIES XCODE_EMBED_APP_EXTENSIONS PushNotif)`.
- [x] Verify `ios/PushNotif/PushNotif.entitlements` exists and contains
      keychain group `fr.calaos.CalaosMobile` + app group
      `group.fr.calaos.CalaosMobile`. Create it if missing.
- [x] Add `ios/ExportOptions.plist` (app‑store, automatic, team
      `9WYC46992U`, `stripSwiftSymbols=true`, no `provisioningProfiles`
      block since signing is automatic).
- [x] Add custom target `archive` running `xcodebuild archive ...` then
      `xcodebuild -exportArchive ... -exportOptionsPlist ios/ExportOptions.plist`.
- [ ] Build locally with `cmake --build build-ios --target archive` →
      verify the resulting `.ipa` installs and registers for APNs.
- [ ] **Regression test**: install the qmake build and the CMake build on
      the same device; verify push notifications decrypt correctly through
      the appex (this is the single highest‑risk item).

### Phase 4 — CI/CD
- [ ] Add `Gemfile` + `fastlane/Fastfile` with lane `beta` (match + gym +
      pilot).
- [ ] Add `.github/workflows/ios-release.yml` (§6.1).
- [ ] Add `.github/workflows/android-release.yml` (§6.2).
- [ ] Document required secrets in `doc/ios.md` and `doc/android.md`.
- [ ] First successful tag‑driven TestFlight + Play internal track
      delivery from CI.

### Phase 5 — Cleanup
- [x] Android: `android/android.pri` deleted, the `android` branch of
      [mobile.pro](../mobile.pro) is an `error()` pointing to
      `tools/build_android.sh` (AND-6).
- [ ] iOS: mark [mobile.pro](../mobile.pro) and [ios/ios.pri](../ios/ios.pri)
      deprecated in their header comment.
- [ ] After one release cycle without bug reports, delete them and remove
      the source‑list duplication test.
- [x] Update [AGENTS.md](../AGENTS.md): Android is CMake only.

---

## 8. Risks & open questions

1. **Source list duplication** between `calaos.pri` and the new CMake list.
   Phase 5 cleanup depends on whether desktop also migrates; until then,
   add a CI check (`tools/check_sources.sh`) that fails on divergence.
2. **PushNotif extension signing**. With automatic signing, Xcode mints a
   second profile `fr.calaos.CalaosMobile.PushNotif` on first archive. CI
   must allow `-allowProvisioningUpdates`. Verify this works on a fresh
   keychain before declaring victory.
3. **Qt 6 Android multi‑ABI builds via CMake** historically required
   per‑ABI configure passes. Qt 6.6+ provides `QT_ANDROID_ABIS` driving a
   single configure → multi‑ABI build via Qt's host‑target separation. Pin
   minimum Qt to **6.6** and document the host‑Qt requirement
   (`QT_HOST_PATH`).
4. **Firebase on Android** is the Java SDK (Gradle); there is no native
   Firebase library to match against Qt's NDK build.
5. **Translations**: `qt_add_translations` requires Qt 6.7+ for the
   `LUPDATE_OPTIONS` arg. If pinned at 6.6, fall back to manual
   `qt_add_lrelease`.
6. **Asset catalog name**: spelled `ressources.xcassets` (French). Don't
   rename it — `AppInfo.plist` and various references depend on it.
7. **Keychain group prefix**: real value at runtime is
   `9WYC46992U.fr.calaos.CalaosMobile` (team prefix). Make sure the
   provisioning profile picked up by automatic signing is associated with
   that team; otherwise the appex cannot read the shared keychain and pushes
   silently fail to decrypt.

---

## 9. Acceptance criteria

The migration is **done** when all of the following hold:

1. `cmake --build build-ios --target archive` produces a signed `.ipa`
   without any human Xcode interaction, on a clean checkout.
2. `cmake --build build-android --target aab` produces a signed AAB
   uploadable to the Play Console, on a clean checkout.
3. Push notifications on iOS decrypt and display correctly (proves the
   appex is built, signed, embedded, and shares the keychain).
4. Push notifications on Android (FCM) deliver and the device token reaches
   `calaos_server` (proves the Firebase setup).
5. `desktop.pro` still builds the desktop variant on Linux + macOS exactly
   as before, with no changes to its toolchain.
6. CI workflows publish to TestFlight and Play Internal Track on tag push.
