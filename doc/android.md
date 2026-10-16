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

Firebase Cloud Messaging through the **Firebase Java SDK** (AND-4; the C++ SDK
is gone). Everything lives in Java under
[android/src/fr/calaos/calaoshome/](../android/src/fr/calaos/calaoshome/);
[android/build.gradle](../android/build.gradle) applies
`com.google.gms.google-services` and depends on `firebase-messaging` (BoM) and
`androidx.work:work-runtime`.

### 6.1 Token

`CalaosMessagingService.onNewToken()` persists the token in the
`calaos_push` SharedPreferences (excluded from backup) and forwards it to C++
only when Qt is running (`HardwareUtils.deliverPushToken`). At startup
`HardwareUtilsAndroid::platformInit()` reads the persisted token, asks
`FirebaseMessaging.getToken()`, and reports it with
`HardwareUtils::setDeviceToken()`; `CalaosConnection::registerPushToken()`
then sends `register_push` (see [mobile.md](mobile.md) section 9).

### 6.2 Contract with calaos_server: data-only messages (AND-7)

With a `notification` block, Play services displays the message itself while
the app is in the background and `onMessageReceived()` never runs, so the
camera picture could not be attached. The app therefore asks for **data-only**
messages. The whole contract is held in `PushFormat` in
[src/CalaosConnection.h](../src/CalaosConnection.h) (key in `JsonKeys.h`):

- `register_push` (websocket and HTTP `api.php`), Android only, adds the field
  **`"push_format": "data"`** next to `token` and `hardware: "android"`.
  iOS and older Android versions send no field and keep the legacy format.
- For a token registered with `push_format: "data"`, calaos_server sends an
  FCM **data message** with `android.priority = "high"` and no
  `notification` block:
  `data = { "title": "Calaos", "body": <message>, "message": <message>, "event_uuid": <uuid> }`
  (all strings; `event_uuid` only when the rule attached a camera picture).
- `CalaosMessagingService.handleMessage()` accepts both shapes
  (`PushPayload.from`): data-only in every app state, legacy
  notification+data while in the foreground (in the background the system
  shows the legacy one, text only, and the data keys reach the activity
  intent on tap).

### 6.3 Camera picture (AND-7)

`handleMessage()` posts the text notification at once (id derived from
`event_uuid`), then enqueues `PushPictureWorker` as an **expedited
WorkManager job** on Android 12+ (unique per event, network constraint). The worker, like
`ios/PushNotif/NotificationService.m`:

1. reads host and credentials from `CredentialStore` (6.4);
2. `GET <api>?cn_user=…&cn_pass=…&action=eventlog&uuid=<event_uuid>` → event
   JSON, `pic_uid`;
3. `GET <api>?cn_user=…&cn_pass=…&action=event_picture&pic_uid=<pic_uid>` →
   the picture (a JPEG from calaos_server; any format `BitmapFactory` decodes,
   first frame of a GIF);
4. downscales it (longest side 1024 px) and re-posts the notification with
   the **same id** in `BigPictureStyle` + `largeIcon`
   (`setOnlyAlertOnce`, so no second sound).

`<api>` is the host as typed: `http(s)://…` used as is, `ws(s)://…` turned
into `http(s)://…`, a bare host becomes `https://<host>/api.php`
(`PushPictureWorker.apiBaseUrl`, same rules as `CalaosConnection::login`).
Timeouts 5 s connect / 10 s read, bodies capped (64 KiB JSON, 8 MiB image),
no redirects (the credentials are in the query string). A network error is
retried twice, each retry being a **new expedited request** (not
`Result.retry()`: WorkManager re-schedules a retried expedited job as a
regular one, which the system batches for minutes). This matters because
the first attempt runs in-process right after the message, while the push
wake-up still grants network; once the process is cached the system closes
its sockets (`SocketException`), and the expedited retry runs as a
scheduler job with its own network access. Any failure (no credentials, unreachable server, HTTP 400 for
wrong credentials, 404, body over the cap, empty `pic_uid`, undecodable
image) leaves the text notification in place without any visible error. A
notification the user dismissed while the download ran is not re-posted
(`NotificationManager.getActiveNotifications()`). Nothing logs a URL, a
credential or image bytes.

Session semantics (decided for AND-7, reversible):

- **Logout** (Settings → Logout, `Application::userLogout()`): the
  credentials stay in the store (login screen prefill) but
  `CredentialStore.setSessionActive(false)` is written and the worker stops
  downloading pictures: pushes show as **text only** until the next
  successful login (`Application::homeLoaded()` sets the flag back). The
  flag is **not** touched by a lost connection, a reconnection, the login
  cancel button or the iOS-style resign-active logout.
- **Reset all data** (`Application::resetAllData()` →
  `resetAuthKeychain()`): full wipe of the store (host, credentials,
  session flag and Keystore key) and of any QSettings leftovers.
- A store without the flag (install updated in the background, never logged
  out explicitly) counts as active.

Limits:

- **TLS is validated by the system trust store**, like `NSURLSession` on iOS.
  A calaos_server with a **self-signed certificate gets the text notification
  only**; the TOFU pinning of the Qt side (`CalaosConnection`) is not shared
  with the worker. Follow-up: export the pinned fingerprint to Java.
- The manifest sets `android:usesCleartextTraffic="true"` so a server
  configured with `http://` / `ws://` works from the Java network stack too
  (Qt's own sockets are not subject to that policy, so the app already talks
  to such servers). The flag applies to every Java network use in the app.
- The worker puts `cn_user`/`cn_pass` in the **query string** (that is what
  `action=eventlog` / `event_picture` accept, as on iOS), whereas the Qt
  side sends them in a POST body: servers and reverse proxies may log the
  URL. Over plain `http://` they travel in clear in both cases.
- Below Android 12 the work is not expedited (expedited work there means a
  foreground service with its own notification): the first attempt still
  runs in-process right away, only the retries are ordinary scheduled jobs.

### 6.4 Credentials readable without Qt: `CredentialStore`

The messaging service and the worker run in a process where the Qt activity
may never have started, so the server address and `cn_user`/`cn_pass` are
stored on the Java side, in the `calaos_credentials` SharedPreferences:

- host in clear (not a secret), credentials encrypted with an **AES-256-GCM
  key of the Android Keystore** (alias `calaos_credentials_v1`, not
  extractable, random IV per write, `base64(iv || ciphertext)`);
  `EncryptedSharedPreferences` is deprecated and not used;
- excluded from Auto Backup and device transfer
  ([res/xml/backup_rules.xml](../android/res/xml/backup_rules.xml),
  [data_extraction_rules.xml](../android/res/xml/data_extraction_rules.xml));
- written by `HardwareUtilsAndroid::saveAuthKeychain()` /
  `setConfigOption("calaos/host")`, read by `loadAuthKeychain()`; wiped
  (values, session flag **and** Keystore key) by `resetAuthKeychain()`,
  i.e. "Reset all data". A plain Logout keeps them and only clears the
  session flag (see 6.3);
- if the Keystore refuses to encrypt a new pair, `saveAuthKeychain()` first
  drops the pair held in the store (`clearCredentials()`, host kept), then
  falls back to QSettings, so the next `loadAuthKeychain()` migrates the
  **new** pair instead of handing out a stale one;
- **migration**: on the first `loadAuthKeychain()` after the update, a pair
  found in QSettings (`calaos/cn_user`, `calaos/cn_pass` in
  `files/settings/Calaos/CalaosHome.conf`) is moved to the store and the
  clear-text copy deleted; the user stays logged in. If the Keystore refuses
  to encrypt, the pair stays in QSettings and the migration is retried at the
  next start. An unreadable value (key lost) reads as "no credentials": the
  user logs in again.

### 6.5 Testing without the server: debug-only fake push

Debug builds (and only them: `sourceSets.debug` in `build.gradle`,
[android/debug/](../android/debug/)) declare `FakePushReceiver`, which
injects a data message into `CalaosMessagingService.handleMessage()`:

```bash
# fake api.php on the Mac (reachable from the emulator as 10.0.2.2)
tools/fake_calaos_api.py --user demo --password demo --picture /tmp/cam.jpg   # --no-picture for the "no pic_uid" case
# in the app: host http://10.0.2.2:8080/api.php, user demo, password demo, then
# (one quoted remote command: adb shell splits words again on the device)
adb shell 'am broadcast -n fr.calaos.calaoshome/.debug.FakePushReceiver \
    --es title Calaos --es body "Motion detected" --es event_uuid 11111111-2222-3333-4444-555555555555'
adb shell cmd statusbar expand-notifications; adb exec-out screencap -p > /tmp/notif.png
```

The receiver requires the `android.permission.DUMP` permission (the adb
shell has it, other apps do not), refuses to run in a non-debuggable process
and does not exist in release builds. `--ez fail_encrypt true` (alone) makes
`CredentialStore.save()` fail until `--ez fail_encrypt false` or a process
restart, to exercise the QSettings fallback.

### 6.6 Manual checklist on a phone (once calaos_server sends data-only messages)

Prerequisites: a calaos_server with the `push_format` change deployed, a
camera rule with push action, the debug APK installed on the phone, logged
in with a user that is **not** the demo one. Watch with
`adb logcat -s CalaosHome`.

1. **Registration**: at login the log shows `Registering push token … for
   android`; on the server the token is stored with the data format
   (server-side check).
2. **App killed**: swipe the app away, trigger the camera rule. Expect, in
   `logcat`, `FCM data message received, event uuid: <uuid>` then
   `Push picture: notification updated for event <uuid>`; on screen a Calaos
   notification with text, which gains the camera picture within a few
   seconds (same entry, no second sound). Tapping it opens the event.
3. **App in background** (home button) and **in foreground**: same result.
4. **Server unreachable**: turn Wi-Fi off / mobile data off on the server
   side (or firewall the server), trigger the rule (or use the fake push
   receiver): text notification only, `Push picture: download failed`, no
   crash, the notification stays.
5. **Wrong credentials**: change the user's password on the server without
   updating the phone, trigger: text only, `server answered HTTP 400`.
6. **Rule without camera**: trigger a push rule with no picture: text only,
   no `event_uuid` in the log, no worker.
7. **Logout**: Settings → Logout, then trigger the camera rule: text
   notification only, `Push picture: user logged out` in the log;
   `run-as fr.calaos.calaoshome cat shared_prefs/calaos_credentials.xml`
   shows `session_active` false and the credentials still there (prefill).
   Log in again: the next event has its picture.
7b. **Reset all data**: Settings → Reset all data. Then
   `adb shell run-as fr.calaos.calaoshome cat shared_prefs/calaos_credentials.xml`
   must be an empty map, and
   `run-as fr.calaos.calaoshome cat files/settings/Calaos/CalaosHome.conf`
   must contain no `cn_user` / `cn_pass`.
8. **Update from a QSettings version**: install the previous build, log in,
   check `cn_user`/`cn_pass` are in `CalaosHome.conf`; install this build
   over it (`adb install -r`), start it: still logged in,
   `credentials migrated to the secure store` in the log, the `.conf` no
   longer has the pair, `shared_prefs/calaos_credentials.xml` holds
   base64 blobs only.
9. **Self-signed server** (if applicable): text notification only; note it
   as the documented limit.

## 7. Signing & deploy

There is **no GitHub Actions workflow** for the Android app — releases are
built locally / via Qt Creator and uploaded by hand. The release flow is
typically:

1. Bump [VERSION.txt](../VERSION.txt): `versionName` is the semver string,
   `versionCode` is `MAJOR*10000 + MINOR*100 + PATCH` (both injected into the
   manifest by `cmake/Versioning.cmake`).
2. Build and sign the release AAB for the four ABIs (CMake build):

   ```bash
   export CALAOS_KEYSTORE=/path/to/upload.jks      # never committed
   export CALAOS_KEY_ALIAS=upload
   read -rs CALAOS_KEYSTORE_PASS; export CALAOS_KEYSTORE_PASS   # prompts, no echo
   # optional, defaults to the store password: CALAOS_KEY_PASS
   ./tools/build_android.sh Release --aab
   ```

   Output: `build-android/CalaosHome-<version>-release.aab`, signed with
   `jarsigner` (passwords go through the environment, never on a command line).
   Without the `CALAOS_*` variables the script warns and leaves the AAB
   unsigned (`build-android/android-build/build/outputs/bundle/release/`).
   `Release` without `--aab` builds the (apksigner-signed) APK instead.
3. Upload to Google Play Console -> CalaosHome (Play App Signing: this is the
   *upload* key).

For local install of a release build: `bundletool build-apks --mode=universal`
on the AAB, or `./tools/build_android.sh Release` and `adb install -r` the APK.

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
## 9. CMake build (preferred)

The CMake path is the new canonical build. It replaces the qmake +
`androiddeployqt` flow and requires no platform-specific path tweaks.

### 9.1 Prerequisites

- Android SDK + NDK (the NDK the Qt kit was built with: 27.2.12479018 for Qt 6.12; see `ndk_version` in the kit's `modules/Core.json`).
- Qt 6.6+ for Android (tested with Qt 6.12): install the `android_arm64_v8a`
  target at minimum, plus the host Qt for your machine.
- `FIREBASE_CPP_SDK_DIR` env set to the Firebase C++ SDK root.
- KDAB [`android_openssl`](https://github.com/KDAB/android_openssl) prebuilt
  cloned to `$HOME/Android/Sdk/android_openssl` (or pass
  `-DANDROID_OPENSSL_DIR=/custom/path`).

### 9.2 Configure (single ABI example)

```bash
export FIREBASE_CPP_SDK_DIR=/path/to/firebase_cpp_sdk

cmake -S . -B build-android -G Ninja \
      -DCMAKE_PREFIX_PATH=/path/to/Qt/6.12.0/android_arm64_v8a \
      -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake \
      -DANDROID_PLATFORM=android-26 \
      -DANDROID_ABI=arm64-v8a \
      -DQT_HOST_PATH=/path/to/Qt/6.12.0/macos
```

For multi-ABI fat APK add `-DQT_ANDROID_ABIS="armeabi-v7a;arm64-v8a;x86;x86_64"`.

### 9.3 Build APK / AAB

```bash
cmake --build build-android --target apk   # unsigned debug APK
cmake --build build-android --target aab   # unsigned AAB for Play Store signing
```

### 9.4 What CMake handles automatically

- Firebase linkage via `cmake/Android.cmake` (fails fast if
  `FIREBASE_CPP_SDK_DIR` is unset).
- OpenSSL extra libs via KDAB's `openssl.cmake`.
- Version name/code injected from [VERSION](../VERSION).
- `ANDROID_PACKAGE_SOURCE_DIR` pointing at [android/](../android/) so
  manifest, Gradle files and Java glue are all picked up as before.

## Legacy qmake build

Sections 2–7 above document the legacy qmake path. It remains functional
during the transition period. New development should target the CMake path.