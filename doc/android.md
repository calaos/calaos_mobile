# Calaos Home — Android build & deploy

> LLM-oriented reference for the **Android** build of `CalaosHome`.
> See [mobile.md](mobile.md) for the cross-platform mobile architecture.
>
> Android is built with **CMake only** (Qt 6, Gradle through `androiddeployqt`).
> The single entry point is [tools/build_android.sh](../tools/build_android.sh).
> qmake no longer supports Android: `qmake mobile.pro` on an Android kit stops
> with an error pointing here. Push notifications use the Firebase Java SDK.

## 1. Identifiers and version matrix

| Setting | Value | Source |
|---|---|---|
| Application id / namespace | `fr.calaos.calaoshome` | [android/build.gradle](../android/build.gradle) |
| Activity | `fr.calaos.calaoshome.HardwareUtils` (extends `QtActivity`) | [android/AndroidManifest.xml](../android/AndroidManifest.xml), [HardwareUtils.java](../android/src/fr/calaos/calaoshome/HardwareUtils.java) |
| App label / icon | `CalaosHome`, `@drawable/icon` | `AndroidManifest.xml`, [android/res/](../android/res) |
| `versionName` / `versionCode` | from [VERSION.txt](../VERSION.txt): semver, and `MAJOR*10000 + MINOR*100 + PATCH` (4.0.0 → 40000) | [cmake/Versioning.cmake](../cmake/Versioning.cmake), injected through `QT_ANDROID_VERSION_NAME/_CODE` |
| Qt | **6.12.0** (kits `android_arm64_v8a`, `android_armv7`, `android_x86`, `android_x86_64` + the host kit `macos`) | `QT_VERSION` in [tools/build_android.sh](../tools/build_android.sh) |
| NDK | **27.2.12479018** (the one the Qt kit was built with: `ndk_version` in the kit's `modules/Core.json`) | read by the script |
| JDK | **17** (Homebrew `openjdk@17`) | the script refuses any other major version |
| Android Gradle Plugin / Gradle | AGP 9.2.1 (`com.google.gms:google-services` 4.5.0) / Gradle 9.5.1 (the wrapper Qt ships in its kit) | [android/build.gradle](../android/build.gradle) |
| minSdk / targetSdk / compileSdk | **28** / **36** / **37.0** (write `37.0`, not `37`: from API 37 platforms carry a minor level) | [cmake/Android.cmake](../cmake/Android.cmake) |
| Java | 17 | `compileOptions` in `build.gradle` |
| OpenSSL | OpenSSL 3, KDAB `android_openssl` (`no-asm` libraries in Debug) | [cmake/Android.cmake](../cmake/Android.cmake) |
| Firebase | Java SDK, BoM 35.0.2 (`firebase-messaging`), `androidx.work:work-runtime` 2.11.2 | `build.gradle` |
| CMake / Ninja | CMake >= 3.25 | [CMakeLists.txt](../CMakeLists.txt) |

`android/build.gradle` and `AndroidManifest.xml` are ported from the Qt 6.12
Android templates, so the Qt kit, AGP and Gradle move together (see 7.1 and
"Rolling back Qt" in 3.4).

## 2. Prerequisites

The script and the defaults below are written for a **macOS** host (Qt host kit
`macos`, Homebrew JDK, SDK in `~/Library/Android/sdk`). Every location can be
overridden by environment variables (3.3). Linux works too (external test of
`tools/build_android.sh Debug`, Qt 6.12.0, NDK 27.2, JDK 17): set
`QT_HOST=$QT_ROOT/gcc_64`, `ANDROID_SDK_ROOT` and `JAVA_HOME` (a JDK 17;
the script only looks for Homebrew when `JAVA_HOME` is unset), and use the
`linux-x86_64` NDK prebuilt directory where this document says `darwin-x86_64`
(4.4). Windows hosts are not supported.

1. **Homebrew tools**: `brew install cmake ninja openjdk@17`. Ninja is also found
   in `~/Qt/Tools/Ninja`.
2. **Qt 6.12.0**, installed with the Qt online installer under `~/Qt/6.12.0`
   (or point `QT_ROOT` at it): the *macOS* host kit plus the Android kits
   *Arm64-v8a*, *ARMv7*, *x86* and *x86_64*. A Debug build needs only
   `android_arm64_v8a`; a Release build needs all four. Qt Creator is optional.
3. **Android SDK** in `~/Library/Android/sdk` (`ANDROID_SDK_ROOT`): install Android
   Studio, or only the command-line tools, then:

   ```bash
   SDK=~/Library/Android/sdk
   "$SDK"/cmdline-tools/latest/bin/sdkmanager \
       "platform-tools" "platforms;android-37.0" "build-tools;36.0.0" "ndk;27.2.12479018"
   ```

   `platforms;android-37.0` is mandatory (CMake configure fails with this exact
   `sdkmanager` command if it is missing). Qt takes the newest installed
   `build-tools`. The NDK must be the one of the Qt kit; the script derives it
   from the kit and fails with "Android NDK not found" otherwise.
4. **KDAB `android_openssl`** (TLS for `QtNetwork` / `QtWebSockets`; the Qt
   Android kits do not ship OpenSSL):

   ```bash
   git clone https://github.com/KDAB/android_openssl.git ~/Library/Android/sdk/android_openssl
   ```

   Any other location works with `ANDROID_OPENSSL_DIR=/path` (the directory that
   contains `android_openssl.cmake`).
5. **Firebase**: [android/google-services.json](../android/google-services.json)
   is in the repo and is bound to the Calaos Firebase project (sender id).
   Replace it with your own if you fork the app, regenerating it from the Firebase
   console; do not hand-edit it. No Firebase SDK has to be installed: Gradle
   downloads the Java libraries (internet access needed on the first build).

## 3. Build

### 3.1 Commands

```bash
git clone https://github.com/calaos/calaos_mobile.git && cd calaos_mobile
./tools/build_android.sh Debug             # APK, arm64-v8a only (default config)
./tools/build_android.sh Release           # APK, 4 ABIs, unsigned unless the CALAOS_* variables are set (5)
./tools/build_android.sh Release --aab     # Play Store bundle, 4 ABIs (5)
./tools/build_android.sh Debug --lib-only  # native libCalaosHome_arm64-v8a.so only, no Gradle
```

Usage: `build_android.sh [Debug|Release] [--aab] [--lib-only]` (`--aab` and
`--lib-only` are mutually exclusive; any other argument is an error). The
build directory is `build-android/` (git-ignored). The script checks its
prerequisites first (Qt kits, SDK, NDK, JDK 17, KDAB OpenSSL, ninja, cmake)
and stops with a precise `ERROR:` line; then it runs `cmake` with the NDK
toolchain (`ANDROID_PLATFORM=android-28`) and `cmake --build`.

### 3.2 Outputs

| Command | Output (the script prints the path at the end) |
|---|---|
| `Debug` | `build-android/android-build/build/outputs/apk/debug/android-build-debug.apk` (debug key) |
| `Release` | an unsigned APK under `build-android/android-build/build/outputs/apk/release/`, or `build-android/CalaosHome-<version>-release.apk` when signed |
| `Release --aab` | `build-android/CalaosHome-<version>-release.aab` when signed; unsigned: `build-android/android-build/build/outputs/bundle/release/` |
| `--lib-only` | `build-android/libCalaosHome_<abi>.so` |

ABIs: Debug builds `arm64-v8a` (fast); Release builds
`arm64-v8a;armeabi-v7a;x86;x86_64`. `CALAOS_ANDROID_ABIS` (semicolon list,
**main ABI first**, its Qt kit is the one CMake is configured with; Qt
configures the others as external projects) overrides it, e.g.
`CALAOS_ANDROID_ABIS=x86_64 ./tools/build_android.sh Debug` for an x86_64
emulator on an Intel host.

### 3.3 Environment overrides

| Variable | Default |
|---|---|
| `QT_VERSION` | `6.12.0` (the only place to bump the Qt version) |
| `QT_ROOT` | `~/Qt/$QT_VERSION` |
| `QT_ANDROID` | `$QT_ROOT/android_<abi of the first CALAOS_ANDROID_ABIS>` |
| `QT_HOST` | `$QT_ROOT/macos` |
| `ANDROID_SDK_ROOT` | `~/Library/Android/sdk` |
| `ANDROID_NDK_ROOT` | `$ANDROID_SDK_ROOT/ndk/<ndk_version of the Qt kit>` |
| `JAVA_HOME` | Homebrew `openjdk@17`, **only when `JAVA_HOME` is unset**: an exported JDK of another major version (for example the JBR 21 of Android Studio) makes the script fail with "JDK 17 required" |
| `ANDROID_OPENSSL_DIR` | `$ANDROID_SDK_ROOT/android_openssl` |
| `CALAOS_ANDROID_ABIS` | `arm64-v8a` (Debug), four ABIs (Release) |
| `CALAOS_KEYSTORE`, `CALAOS_KEYSTORE_PASS`, `CALAOS_KEY_ALIAS`, `CALAOS_KEY_PASS` | release signing, see 5 |

### 3.4 Good to know

- Switching Qt kit (version bump or rollback): the script detects it and deletes
  `build-android/CMakeCache.txt`, `CMakeFiles`, `android-build/` and
  `android_abi_builds/` itself. After an SDK/NDK change that breaks configure,
  delete them by hand (the toolchain paths are cached).
- `androiddeployqt` never removes stale files. After removing something from
  `android/` or changing what is deployed, `rm -rf build-android/android-build`.
- **Rolling back Qt** is not knob-only on Android: `android/build.gradle` is
  ported from the 6.12 template (AGP 9.2.1, needs Gradle >= 9.4.1, whereas the
  6.11 kit ships Gradle 9.3.1). For 6.11: revert the commit "build: port the Qt
  6.12 Android templates (QT-1)", then `QT_VERSION=6.11.0 ./tools/build_android.sh`.
  The script refuses a kit whose template AGP differs from `android/build.gradle`.
  iOS rolls back with `QT_VERSION` alone.
- Qt Creator can open [CMakeLists.txt](../CMakeLists.txt) with the Android kits,
  but the script is the reference (kit/NDK/JDK checks, multi-ABI, signing).

## 4. Install, run and debug

### 4.1 On a phone

Enable *Developer options* → *USB debugging* (Xiaomi/MIUI/HyperOS: also *Install
via USB*, and accept the on-screen confirmation of each install of a new
package, otherwise `INSTALL_FAILED_USER_RESTRICTED: Install canceled by user`).

```bash
export PATH="$HOME/Library/Android/sdk/platform-tools:$PATH"      # adb
adb devices                                  # must say "device", not "unauthorized"
adb install -r build-android/android-build/build/outputs/apk/debug/android-build-debug.apk
adb shell am start -n fr.calaos.calaoshome/fr.calaos.calaoshome.HardwareUtils
```

If the Play Store build of `fr.calaos.calaoshome` (release-signed) is installed,
`adb install -r` fails with `INSTALL_FAILED_UPDATE_INCOMPATIBLE`; uninstalling
it deletes the app data (host, credentials), so only do it knowingly.

### 4.2 On an emulator

Create an AVD from Android Studio's Device Manager (API 28 or newer) with an
image whose ABI you built: arm64-v8a on Apple Silicon (the default Debug build),
x86_64 on Intel hosts (`CALAOS_ANDROID_ABIS=x86_64 ./tools/build_android.sh Debug`).
Start it and install as above. From the emulator the host machine is `10.0.2.2`.

### 4.3 Logs

```bash
adb logcat -c; adb logcat --pid="$(adb shell pidof -s fr.calaos.calaoshome)"   # app only
adb logcat -s CalaosHome                    # the app's Java and C++ log tag
adb logcat -b crash                         # native / Java crashes
```

Never paste logs containing credentials: the app does not log `cn_user`,
`cn_pass`, full push tokens or URLs carrying them, and new code must not either.

### 4.4 Useful checks on a built package

```bash
APK=build-android/android-build/build/outputs/apk/debug/android-build-debug.apk
BT=$HOME/Library/Android/sdk/build-tools/36.0.0
"$BT"/aapt2 dump permissions "$APK"                 # permissions of the merged manifest
"$BT"/zipalign -c -P 16 -v 4 "$APK" | tail -1       # 16 KB page-size alignment
```

16 KB pages (Play Store requirement for Android 15+): 64-bit libraries
(arm64-v8a, x86_64) are linked with `LOAD` alignment `0x4000`; the 32-bit ones
(armeabi-v7a, x86) stay `0x1000`, which is fine because 16 KB devices are
64-bit only. Check a library with
`$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-readelf -lW lib.so | grep LOAD`
(`linux-x86_64` instead of `darwin-x86_64` on Linux).

## 5. Release: AAB and signing

There is no GitHub Actions workflow for Android: releases are built locally
and uploaded by hand.

1. Bump [VERSION.txt](../VERSION.txt) (`versionName` = the semver string,
   `versionCode` derived, both injected into the manifest by CMake).
2. Have an **upload keystore** outside the repository (never commit it nor its
   passwords). To create one:

   ```bash
   keytool -genkeypair -v -storetype PKCS12 -keystore upload.p12 -alias upload \
       -keyalg RSA -keysize 2048 -validity 10000
   ```

   (for a throwaway test key do it in `/tmp` and delete it afterwards). A PKCS12
   keystore has a single password: the script rejects a `CALAOS_KEY_PASS`
   different from the store password (only a JKS store can have distinct
   passwords).
3. Build and sign:

   ```bash
   export CALAOS_KEYSTORE=/path/to/upload.jks      # never committed
   export CALAOS_KEY_ALIAS=upload
   read -rs CALAOS_KEYSTORE_PASS; export CALAOS_KEYSTORE_PASS   # prompts, no echo
   # optional, defaults to the store password: CALAOS_KEY_PASS
   ./tools/build_android.sh Release --aab
   ```

   Output: `build-android/CalaosHome-<version>-release.aab`, about 127 MB (Qt
   libraries stored uncompressed, four ABIs); Play delivers per-ABI splits
   (about 31 MB for arm64-v8a). `Release` without `--aab` signs the APK with
   `apksigner` instead (`build-android/CalaosHome-<version>-release.apk`).
4. Signing is done **by the script after the build**, not by Gradle or
   `androiddeployqt`: a Gradle `signingConfig` renames the APK and makes
   `androiddeployqt` fail, and would hand the passwords to the long-lived
   Gradle daemon. The script validates keystore, alias and key password with
   `keytool` **before** the build, then signs a copy with `jarsigner`
   (`-storepass:env`, AAB) or `apksigner` (`--ks-pass env:`, APK); the passwords
   only travel through the signer's own environment, never on a command line.
   The `CALAOS_*` variables are all-or-nothing (`CALAOS_KEY_PASS` optional): a
   partial set is an error; none at all gives a loud warning and an unsigned,
   non-uploadable package.
5. Check: `"$JAVA_HOME"/bin/jarsigner -verify build-android/CalaosHome-*-release.aab`
   (the script already does it), and `aapt2 dump permissions` on a release APK
   (`WRITE_EXTERNAL_STORAGE` is removed from release builds by
   [android/release/AndroidManifest.xml](../android/release/AndroidManifest.xml);
   debug keeps it). Release builds contain no `FakePushReceiver` (6.5).
6. Upload the AAB to the Google Play Console → CalaosHome. Play App Signing is
   enabled: this key is only the *upload* key.

To install a release bundle on a phone, create a universal APK with
[bundletool](https://github.com/google/bundletool) (`build-apks --mode=universal`
with a keystore of your choice); signing it with `~/.android/debug.keystore`
(alias `androiddebugkey`, password `android`) lets it replace a debug build
with `adb install -r` without losing app data. Or simply
`./tools/build_android.sh Release` and `adb install -r` the APK.

## 6. Push notifications (FCM)

Firebase Cloud Messaging through the **Firebase Java SDK** (AND-4).
Everything lives in Java under
[android/src/fr/calaos/calaoshome/](../android/src/fr/calaos/calaoshome/);
[android/build.gradle](../android/build.gradle) applies
`com.google.gms.google-services` and depends on `firebase-messaging` (BoM) and
`androidx.work:work-runtime`.

### 6.1 Token and notification tap

`CalaosMessagingService.onNewToken()` persists the token in the
`calaos_push` SharedPreferences (excluded from backup) and forwards it to C++
only when Qt is running (`HardwareUtils.deliverPushToken`). At startup
`HardwareUtilsAndroid::platformInit()` reads the persisted token, asks
`FirebaseMessaging.getToken()`, and reports it with
`HardwareUtils::setDeviceToken()`; `CalaosConnection::registerPushToken()`
then sends `register_push` (see [mobile.md](mobile.md) section 9).

**Tap hand-off.** Tapping a notification starts (or re-enters, the activity is
`singleTop`) `HardwareUtils` with the event uuid as intent extra. The extra is
untrusted (the activity is exported): it is validated, and an intent replayed
from Recents is ignored. If Qt is already running (`setNativesReady()` was
called) the activity calls `HardwareUtilsNatives.emitNotificationOpened()`
straight away. On a cold start it parks the uuid in a static field instead;
`HardwareUtilsAndroid::platformInit()` takes it with `takePendingEventUuid()`
(cleared on read). Both paths end in `HardwareUtils::handleNotificationOpened()`,
which emits `pushNotifReceived` at once when connected, or keeps the uuid until
the home is loaded (not logged in yet).

### 6.2 Contract with calaos_server: data-only messages (AND-7)

With a `notification` block, Play services displays the message itself while
the app is in the background and `onMessageReceived()` never runs, so the
camera picture could not be attached. The app therefore asks for **data-only**
messages. The whole contract is held in `PushFormat` in
[src/CalaosConnection.h](../src/CalaosConnection.h) (key in `JsonKeys.h`):

- `register_push` (websocket and HTTP `api.php`), Android only, adds the field
  **`"push_format": "data"`** next to `token` and `hardware: "android"`.
  iOS and older app versions send no field and keep the legacy format.
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
  cancel button or the iOS resign-active pause.
- **Reset all data** (`Application::resetAllData()` →
  `resetAuthKeychain()`): full wipe of the store (host, credentials,
  session flag and Keystore key) and of any QSettings leftovers.
- A store without the flag (install updated in the background, never logged
  out explicitly) counts as active.

Limits:

- **TLS**: a certificate the system trust store accepts for the host is used
  as is. Otherwise (self-signed box, certificate for another name) the worker
  accepts only the key the user trusted in the app: C++ copies the pins
  (`host:port` -> SHA-256 of the SubjectPublicKeyInfo, `src/TlsPinning.h`) to
  `CredentialStore` (`tls_pins`, JSON, not encrypted: not a secret), and the
  worker checks `PublicKey.getEncoded()` of the leaf against it, for the chain
  and for the host name. A pinned host must present its pinned key even when
  the system trusts the certificate. No pin or another key: text
  notification only (the worker cannot ask the user).
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
tools/fake_calaos_api.py --user demo --password demo --picture /tmp/cam.jpg   # any JPEG; --picture stays required with --no-picture (the "no pic_uid" case)
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
   `event uuid: <none>` in the log, no worker.
7. **Logout**: Settings → Logout, then trigger the camera rule: text
   notification only, `Push picture: user logged out` in the log;
   `adb shell run-as fr.calaos.calaoshome cat shared_prefs/calaos_credentials.xml`
   shows `session_active` false and the credentials still there (prefill).
   Log in again: the next event has its picture.
8. **Reset all data**: Settings → Reset all data. Then
   `adb shell run-as fr.calaos.calaoshome cat shared_prefs/calaos_credentials.xml`
   must be an empty map, and
   `adb shell run-as fr.calaos.calaoshome cat files/settings/Calaos/CalaosHome.conf`
   must contain no `cn_user` / `cn_pass`.
9. **Update from a QSettings version**: install the previous build, log in,
   check `cn_user`/`cn_pass` are in `CalaosHome.conf`; install this build
   over it (`adb install -r`), start it: still logged in,
   `credentials migrated to the secure store` in the log, the `.conf` no
   longer has the pair, `shared_prefs/calaos_credentials.xml` holds
   base64 blobs only.
10. **Self-signed server** (if applicable): picture shown once the certificate
   was trusted in the app; text notification only before that, or after the
   pin was forgotten in Settings.

## 7. Project layout and packaging

### 7.1 What is in `android/`

`QT_ANDROID_PACKAGE_SOURCE_DIR` is [android/](../android): every file there
**replaces** the file Qt would generate. Keep only what is customised:

| Path | Role |
|---|---|
| `AndroidManifest.xml` | Qt 6.12 template + our components; no `package=` attribute (AGP 8+ uses `namespace`); keep the `-- %%INSERT_...%% --` placeholders and the `<!-- %%INSERT_PERMISSIONS -->` / `<!-- %%INSERT_FEATURES -->` markers that `androiddeployqt` fills (permissions come from the Qt modules, do not hand-edit them; custom ones go next to the markers). No `--` inside XML comments (it breaks `androiddeployqt`) |
| `build.gradle` | Qt 6.12 template + Firebase / WorkManager dependencies, `debug` and `release` source sets. It must consume the values `androiddeployqt` writes into `gradle.properties` (`androidCompileSdkVersion`, `androidBuildToolsVersion`, `androidNdkVersion`, `qtMinSdkVersion`, `qtTargetSdkVersion`, `qtTargetAbiList`, `qtAndroidDir`, ...), never hard-code them |
| `src/fr/calaos/calaoshome/*.java` | `HardwareUtils` (activity), `CalaosMessagingService`, `PushPictureWorker`, `CredentialStore` (8, 6) |
| `debug/` | debug-only `AndroidManifest.xml` + `FakePushReceiver.java` (6.5) |
| `release/AndroidManifest.xml` | release-only overlay removing `WRITE_EXTERNAL_STORAGE` |
| `res/` | icons, `ic_stat_calaos` notification icon, colours, backup rules (`res/xml/backup_rules.xml`, `data_extraction_rules.xml`) |
| `google-services.json` | Firebase configuration (2) |
| `HardwareUtils_Android.cpp/.h` | the C++ side of the JNI bridge, registered in [cmake/Android.cmake](../cmake/Android.cmake) |

Never add a Gradle wrapper, `settings.gradle`, `gradle.properties`,
`project.properties` or `res/values/libs.xml` to `android/`: Qt 6 generates its
own, and a stale copy breaks the build or the loading of the Qt libraries.

### 7.2 Android specifics of the build

- **Compile/target SDK**: set as target properties in `cmake/Android.cmake`
  (`QT_ANDROID_MIN_SDK_VERSION` 28, `QT_ANDROID_TARGET_SDK_VERSION` 36,
  `QT_ANDROID_COMPILE_SDK_VERSION` 37.0). Raising the target SDK needs a review of
  the Android behaviour changes of that release.
- **Deployed plugins and QML modules**: `qt_import_plugins(... EXCLUDE ...)` drops
  plugins the app never loads (qmltooling outside Debug, virtual keyboard input
  context, icon engines, offscreen platform, icns/ico/tga/tiff/wbmp).
  `calaos_android_limit_qml_scan()` restricts the `androiddeployqt` QML import
  scan to `qml/mobile`, `qml/SharedComponents` and `qml/quickflux` (same list as
  `cmake/qml_imports` for iOS; keep both in sync with `qml_mobile.qrc` and
  `qml_shared.qrc`). Check the result in
  `build-android/android-CalaosHome-deployment-settings.json`
  (`android-deploy-plugins`, `qml-root-path`). The four unused Quick Controls
  styles stay: they are listed in the generated `libs.xml` and loaded at startup.
- **Shared sources** stay shared: a new C++ file goes in `calaos.pri` **and**
  `cmake/CalaosHomeSources.cmake` (`tools/check_sources.sh` checks they match);
  Android-only C++ goes in `android/` and `cmake/Android.cmake`.
- **R8 / minify is off** (`minifyEnabled false`): Qt's Java is resolved by name
  from native code. If enabled, keep every JNI-by-name target (`HardwareUtils`
  static helpers, `HardwareUtilsNatives`, `CredentialStore`, the manifest
  components `CalaosMessagingService`, `PushPictureWorker`) and test a release
  APK on a device.
- **Native libs are stored uncompressed and page-aligned** in the APK
  (`legacyPackaging=false`, Qt 6.12): a debug APK is larger than with Qt 6.11,
  Play compresses downloads anyway.

### 7.3 Behaviours worth knowing

- **Back key**: Qt delivers back as `Qt::Key_Back`; if QML does not accept the
  *release*, Qt moves the task to the background instead of finishing the activity
  (`qml/mobile/main.qml` leaves it unaccepted at the root on purpose). On Android
  13+ a back sent as a key event (`adb shell input keyevent KEYCODE_BACK`)
  arrives twice (press, press, release, release), which `main.qml` handles by
  counting non-repeat presses and acting on the last release. A rare Qt 6.12 race
  can still background the task although QML accepted the release; gesture and
  navigation-bar back are not affected. Test with `input keyevent`, an edge swipe
  (`adb shell input swipe 2 1200 600 1200 300`, gesture mode) and 3-button mode
  (`adb shell cmd overlay enable com.android.internal.systemui.navbar.threebutton`,
  `...navbar.gestural` to go back).
- **Navigation bar**: `HardwareUtils` turns off the navigation-bar contrast scrim
  (API 29+) and, from API 35, keeps light navigation icons on the dark menu bar.
  Check with `adb shell dumpsys window | grep mLastAppearance` (no
  `LIGHT_NAVIGATION_BARS` in system light mode: `adb shell cmd uimode night no`).
  Safe areas / edge-to-edge are described in [mobile.md](mobile.md).
- **Network changes**: `HardwareUtilsAndroid` uses Qt's `QNetworkInformation`
  Android backend and falls back to the Java `ConnectivityManager` query when it
  reports `Unknown` (handovers). The emulated network can flap under heavy host load.
- **Single instance**: the activity is `singleTop`; a push intent re-entering the
  running app reuses the instance.

## 8. Java ↔ C++ bridge

Conventions (QJniObject, native-method registration, threading, code that runs
without Qt) are in the `qt6-android-jni` skill. The Java helpers called from
C++ (`HardwareUtilsAndroid`, [android/HardwareUtils_Android.cpp](../android/HardwareUtils_Android.cpp))
are static methods of `HardwareUtils`:

| Java method | C++ caller / purpose |
|---|---|
| `showAlertMessage(title, msg, button)` | `HardwareUtilsAndroid::showAlertMessage` |
| `getNetworkStatus()` | `HardwareUtilsAndroid::getNetworkStatus` (fallback) |
| `inputtextDialog(title, msg, ...)` | `HardwareUtilsAndroid::inputTextDialog` |
| `getStoredPushToken()`, `requestPushToken()` | push token at startup (6.1) |
| `takePendingEventUuid()` | event of the notification that started the app (6.1, tap hand-off) |
| `setNativesReady()` | Qt is up: Java may call back |

Java → C++ callbacks are the `native` methods of `HardwareUtilsNatives`
(`emitDialogTextValid`, `emitDialogCancel`, `emitDeviceToken`,
`emitNotificationOpened`), registered by `HardwareUtilsAndroid::platformInit()`
with `QJniEnvironment::registerNativeMethods`. They only exist once Qt runs:
code that can run in a process started for a push (`CalaosMessagingService`,
`PushPictureWorker`) must not call them unguarded (6.1, 6.4). Keystore-backed
credentials are described in 6.4.

## 9. Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `ERROR: Qt Android kit not found` / `Qt kit for ABI ... not found` | install the missing Qt 6.12 Android kit, or set `QT_ROOT` / `CALAOS_ANDROID_ABIS` |
| `ERROR: JDK 17 not found` / `JDK 17 required, found major version ...` | `brew install openjdk@17`, or `JAVA_HOME` of a JDK 17 |
| `ERROR: Android NDK not found` | `sdkmanager "ndk;27.2.12479018"` (or the NDK version of your kit) |
| `ERROR: KDAB android_openssl not found` | clone it (2, step 4) or set `ANDROID_OPENSSL_DIR` |
| CMake: `platform android-37.0 not found` | `sdkmanager "platforms;android-37.0"` |
| `android/build.gradle uses AGP ... but the Qt kit template ...` | the Qt kit and the ported templates differ: see "Rolling back Qt" (3.4) |
| Configure fails after an SDK/NDK/Qt change | `rm -rf build-android/CMakeCache.txt build-android/CMakeFiles build-android/android-build` |
| Strange Gradle / library-loading failure after removing files from `android/` | `rm -rf build-android/android-build` |
| `Expected '>', but got ' '` from `androiddeployqt` | a `--` inside an XML comment of `AndroidManifest.xml` |
| `INSTALL_FAILED_UPDATE_INCOMPATIBLE` | a differently signed build is installed (4.1) |
| `INSTALL_FAILED_USER_RESTRICTED` (MIUI) | accept the install prompt on the phone; enable *Install via USB* |
| Release build says UNSIGNED | the `CALAOS_*` variables are not set (5) |
| `incomplete signing setup` / `cannot open the keystore` | all of `CALAOS_KEYSTORE`, `CALAOS_KEYSTORE_PASS`, `CALAOS_KEY_ALIAS` are needed; check store password and alias |

## 10. Things an LLM should remember

1. Android builds **only** through `tools/build_android.sh` (CMake +
   `androiddeployqt` + Gradle). `mobile.pro` has no Android branch: it ends with
   an `error()` pointing to the script. Desktop (`desktop.pro`) and iOS are
   untouched by Android changes; guard platform code with `if(ANDROID)` in CMake
   and `#ifdef Q_OS_ANDROID` in C++.
2. New Java sources go under
   [android/src/fr/calaos/calaoshome/](../android/src/fr/calaos/calaoshome/) and are
   picked up automatically (`QT_ANDROID_PACKAGE_SOURCE_DIR`). New native files are
   added to `cmake/Android.cmake`, not to `calaos.pri`.
3. `android/` overrides Qt's templates (7.1): do not add Gradle wrapper,
   `libs.xml`, `settings.gradle` or `gradle.properties`.
4. Manifest permissions are generated from the Qt modules; the app adds
   `POST_NOTIFICATIONS` itself and removes `FOREGROUND_SERVICE`,
   `RECEIVE_BOOT_COMPLETED` (and `WRITE_EXTERNAL_STORAGE` in release).
5. SDK levels are set in `cmake/Android.cmake`, not in `build.gradle`; bumping
   the target SDK may need Java glue changes (notification channels,
   foreground-service rules, back handling).
6. Don't hand-edit [android/google-services.json](../android/google-services.json);
   regenerate it from the Firebase console.
7. Any change touching `HardwareUtils`, `CredentialStore`, the push classes or the
   manifest: run the checklist of 6.6 on a phone, and re-check the `release`
   build (no R8, JNI names 7.2).
8. Never commit keystores or passwords, never log credentials, full push tokens
   or URLs carrying `cn_user` / `cn_pass`.
