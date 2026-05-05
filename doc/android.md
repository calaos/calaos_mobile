# Calaos Home — Android build & deploy

> LLM-oriented reference for the **Android** build of `CalaosHome`.
> See [mobile.md](mobile.md) for the cross-platform mobile architecture.

## 1. Identifiers

| Setting | Value | Source |
|---|---|---|
| Application id / package | `fr.calaos.calaoshome` | [android/build.gradle](../android/build.gradle), [android/AndroidManifest.xml](../android/AndroidManifest.xml) |
| Activity | `fr.calaos.calaoshome.HardwareUtils` (extends `QtActivity`) | [android/src/fr/calaos/calaoshome/HardwareUtils.java](../android/src/fr/calaos/calaoshome/HardwareUtils.java) |
| Min SDK | 26 (Android 8.0) | [android/build.gradle](../android/build.gradle) |
| Target SDK | 29 (Android 10) | [android/build.gradle](../android/build.gradle) |
| Java compatibility | 1.8 | `build.gradle` `compileOptions` |
| App label | `CalaosHome` | `AndroidManifest.xml` |
| Icon | `@drawable/icon` | [android/res/](../android/res) |
| AndroidX | enabled | [android/gradle.properties](../android/gradle.properties) |

`versionName` / `versionCode` are kept in
[android/AndroidManifest.xml](../android/AndroidManifest.xml). Bump them before
each Play Store release.

## 2. Required external SDKs

The Android build pulls **two** out-of-tree dependencies that must be installed
before running qmake:

### 2.1 Firebase C++ SDK

[android/android.pri](../android/android.pri) requires the env var
`FIREBASE_CPP_SDK_DIR`:

```qmake
FIREBASE_SDK = $$(FIREBASE_CPP_SDK_DIR)
isEmpty(FIREBASE_SDK) {
    error("FIREBASE_CPP_SDK_DIR env is not set. Please set it first")
}

INCLUDEPATH += $$FIREBASE_SDK/include
LIBS += -L$$FIREBASE_SDK/libs/android/$$ANDROID_TARGET_ARCH \
        -lfirebase_messaging -lfirebase_app -lfirebase_auth
```

Download from <https://firebase.google.com/download/cpp>. Set:

```bash
export FIREBASE_CPP_SDK_DIR=/path/to/firebase_cpp_sdk
```

[android/build.gradle](../android/build.gradle) reads the same variable through
`System.getenv('FIREBASE_CPP_SDK_DIR')` and applies
`firebase_dependencies.gradle` from it; it then declares only the `messaging`
component.

### 2.2 Android OpenSSL prebuilt

```qmake
include(C:/Users/$$(USERNAME)/AppData/Local/Android/Sdk/android_openssl/openssl.pri)
```

The path is hard-coded for Windows. On Linux/macOS, edit it to your local copy
of [`android_openssl`](https://github.com/KDAB/android_openssl) before building,
e.g.:

```qmake
include($$(HOME)/Android/Sdk/android_openssl/openssl.pri)
```

This is required so that `QtNetwork` / `QtWebSockets` can perform TLS to
`calaos_server`.

### 2.3 Firebase configuration

[android/google-services.json](../android/google-services.json) is shipped in
the repo. Replace it with your own if you fork — the bundled file is bound to
the Calaos Firebase project that holds the FCM sender id and APNs forwarding
config.

## 3. Build

### 3.1 qmake / androiddeployqt flow

The Qt build pipeline produces a `libCalaosHome_<arch>.so` per ABI, then
invokes `androiddeployqt` to assemble an APK / AAB by combining:

- [android/AndroidManifest.xml](../android/AndroidManifest.xml)
- [android/build.gradle](../android/build.gradle)
- [android/gradle.properties](../android/gradle.properties),
  [android/settings.gradle](../android/settings.gradle)
- [android/res/](../android/res) (icons, drawables, libs.xml)
- Java sources in [android/src/](../android/src) and the Qt-shipped
  `qt5AndroidDir + '/src'`.

`ANDROID_PACKAGE_SOURCE_DIR = $$PWD` is set in
[android/android.pri](../android/android.pri) so all those files are picked up.

### 3.2 Command-line build

```bash
export ANDROID_HOME=/opt/android-sdk
export ANDROID_NDK_ROOT=/opt/android-ndk
export FIREBASE_CPP_SDK_DIR=/opt/firebase_cpp_sdk
# Edit the openssl include path in android/android.pri to match your system.

# Set up a Qt 6 Android shadow build:
mkdir -p build-android && cd build-android
~/Qt/<ver>/android_arm64_v8a/bin/qmake ../mobile.pro \
    "ANDROID_ABIS=arm64-v8a" CONFIG+=release
make -j$(nproc)
make apk          # produces build/outputs/apk/...
# or:
make aab          # produces an Android App Bundle for the Play Store
```

For multi-ABI builds set `ANDROID_ABIS="armeabi-v7a arm64-v8a x86 x86_64"`.

### 3.3 Qt Creator build

Qt Creator manages `gradle.properties`, `qt5AndroidDir`,
`androidBuildToolsVersion`, `androidCompileSdkVersion` automatically. The
warning at the top of [android/build.gradle](../android/build.gradle) is
explicit: do not edit those values manually.

## 4. Manifest highlights

[android/AndroidManifest.xml](../android/AndroidManifest.xml):

```xml
<application android:hardwareAccelerated="true"
             android:name="org.qtproject.qt.android.bindings.QtApplication"
             android:label="CalaosHome"
             android:requestLegacyExternalStorage="true"
             android:allowNativeHeapPointerTagging="false"
             android:icon="@drawable/icon">
    <activity android:name="fr.calaos.calaoshome.HardwareUtils"
              android:launchMode="singleTop"
              android:exported="true"
              android:configChanges="orientation|uiMode|screenLayout|...">
        ...
        <meta-data android:name="android.app.background_running" android:value="false"/>
        <meta-data android:name="android.app.extract_android_style" android:value="minimal"/>
    </activity>
</application>
```

- `allowNativeHeapPointerTagging=false` is required for older Firebase / Qt
  combos.
- `requestLegacyExternalStorage=true` — kept for compatibility with Calaos
  servers that expose media via legacy paths.
- `extract_android_style = minimal` — recommended for Quick Controls 2 apps.
- The `<!-- %%INSERT_PERMISSIONS -->` placeholder is filled by
  `androiddeployqt` based on Qt module dependencies; do not hand-edit
  permissions.

## 5. Java glue layer

[android/src/fr/calaos/calaoshome/HardwareUtils.java](../android/src/fr/calaos/calaoshome/HardwareUtils.java)
extends `QtActivity` and is set as the launcher activity. It exposes static
helpers callable from C++ via JNI:

| Java method | C++ caller |
|---|---|
| `showAlertMessage(title, msg, button)` | `HardwareUtilsAndroid::showAlertMessage` |
| `getNetworkStatus()` | `HardwareUtilsAndroid::getNetworkStatus` |
| `inputtextDialog(title, msg)` | `HardwareUtilsAndroid::inputTextDialog` |

Java → C++ callbacks go through native methods declared on
`HardwareUtilsNatives` (`emitDialogTextValid`, `emitDialogCancel`).

## 6. Push notifications (FCM)

Implementation in
[android/HardwareUtils_Android.cpp](../android/HardwareUtils_Android.cpp):

1. `HardwareUtilsAndroid::platformInit()` instantiates `FbListener`, which
   creates a `firebase::App` from the JNI activity context.
2. `firebase::messaging::Initialize(...)` registers the listener.
3. `firebase::messaging::GetToken()` future delivers the FCM token →
   `HardwareUtilsAndroid::setDeviceToken()` → `HardwareUtils::deviceToken`.
4. `OnTokenReceived` and `OnMessage` are overridden to keep the token in sync
   and (currently) log incoming messages.

[android/build.gradle](../android/build.gradle) applies
`com.google.gms.google-services` + the Firebase C++ `firebase_dependencies.gradle`
with `firebaseCpp.dependencies { messaging }`.

## 7. Signing & deploy

There is **no GitHub Actions workflow** for the Android app — releases are
built locally / via Qt Creator and uploaded by hand. The release flow is
typically:

1. Bump `versionName` + `versionCode` in
   [android/AndroidManifest.xml](../android/AndroidManifest.xml).
2. Build a release AAB (`make aab`).
3. Sign with the Calaos upload keystore (not stored in this repo).
4. Upload to Google Play Console → CalaosHome.

For local install:

```bash
adb install -r build/outputs/apk/release/calaos_mobile-release.apk
```

## 8. Things an LLM should remember

1. The Android target **requires both** `FIREBASE_CPP_SDK_DIR` and a working
   `android_openssl` include — without them, qmake fails immediately. The
   `openssl.pri` path is hard-coded for Windows and **must** be adapted on
   Linux/macOS.
2. New Java sources go under
   [android/src/fr/calaos/calaoshome/](../android/src/fr/calaos/calaoshome/) and
   are picked up automatically because of `ANDROID_PACKAGE_SOURCE_DIR = $$PWD`.
3. New native files (C++/JNI bridges) must be added to
   [android/android.pri](../android/android.pri), not to
   [calaos.pri](../calaos.pri).
4. Manifest permissions are auto-generated by `androiddeployqt`; if you need a
   *custom* permission, add it next to (not inside) the
   `<!-- %%INSERT_PERMISSIONS -->` placeholder.
5. `compileSdkVersion` / `buildToolsVersion` come from
   [android/gradle.properties](../android/gradle.properties) — Qt Creator
   rewrites them, so do not commit local edits.
6. `min/targetSdkVersion` are static in
   [android/build.gradle](../android/build.gradle) — bumping them may require
   adapting Java glue (e.g., notification channels).
7. The native activity is `singleTop` — when the app is already running and a
   push intent re-enters it, the existing instance is reused. Lifecycle
   handling in `Application` (logout on resign-active, login on become-active)
   relies on this.
8. Don’t hand-edit [android/google-services.json](../android/google-services.json)
   unless you regenerate it from the Firebase console.
