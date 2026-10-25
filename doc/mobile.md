# Calaos Home — Mobile variant

> LLM-oriented reference for the **iOS / Android / desktop-test** mobile build.
> Companion docs: [desktop.md](desktop.md), [ios.md](ios.md), [android.md](android.md).

## 1. Purpose & target

The mobile build is the official **iOS** (App Store) and **Android** (Play Store)
client for a Calaos installation. It is also buildable on Linux/macOS desktop for
quick UI iteration (in that fallback case `QApplication` is used and `widgets` is
pulled in).

The variant is selected at build time by the `CALAOS_MOBILE` preprocessor define
and either the [mobile.pro](../mobile.pro) qmake project file (desktop test build,
iOS) or the CMake build ([CMakeLists.txt](../CMakeLists.txt): Android, iOS).

## 2. Build

### 2.1 Required Qt modules

From [mobile.pro](../mobile.pro):

```
QT += qml quick network gui websockets svg
```

The desktop test build additionally pulls `widgets`. C++ standard: C++11. Targets Qt 6.

### 2.2 Build commands

```bash
# Generic (desktop test mode)
qmake mobile.pro
make -j$(nproc)

# Android (CMake only): ./tools/build_android.sh Debug, see android.md
# iOS:                  see ios.md
```

Android is **not** supported by qmake: the `android` branch of `mobile.pro` is an
`error()` pointing to `tools/build_android.sh`. The rest of the file dispatches
to platform sub-projects:

```qmake
android { error("... use ./tools/build_android.sh ...") }
else: ios { include(ios/ios.pri) }
else { QT += widgets }
```

### 2.3 Runtime entry

`src/main.cpp` is shared with desktop. On Android it sets
`QT_QPA_NO_TEXT_HANDLES=1` (works around stray text selection handles in
`HomeView`). On both Android and iOS, the application class derives from
`QGuiApplication` (see `QAPP` macro in [src/Application.h](../src/Application.h)).

## 3. Resources & QML entry point

QML root: [qml/mobile/main.qml](../qml/mobile/main.qml). Imported QML modules:
`Calaos`, `SharedComponents`, plus the local `quickflux` directory.

Resource files compiled in (from [mobile.pro](../mobile.pro)):

- [qml_mobile.qrc](../qml_mobile.qrc) — mobile-specific QML
- [qml_shared.qrc](../qml_shared.qrc) — `SharedComponents` module
- [img_mobile.qrc](../img_mobile.qrc), [fonts_mobile.qrc](../fonts_mobile.qrc)
- [lang.qrc](../lang.qrc) — translations

The window minimum size is `320 × 480 dp` scaled by `calaosApp.density`. On
Android `density` is hard-coded to `1` (see comment in
[Application.cpp](../src/Application.cpp)) and the back button is disabled
because the system already provides one (`needBackButton = false`).

## 4. Mobile-specific properties exposed to QML

`Application` exposes platform flags as `QML_READONLY_PROPERTY` (see
[Application.h](../src/Application.h)):

| Property | Value on iOS | Value on Android | Value on desktop test |
|---|---|---|---|
| `isAndroid` | false | true | false |
| `isIOS` | true | false | false |
| `isDesktop` | false | false | true |
| `needBackButton` | true | false | true |
| `hasWebEngine` | false | false | depends |
| `hasInstall` | false | false | only if live medium markers exist |
| `density` | 1.0 (or 2.0 with `--force-hdpi`) | 1.0 | 1.0/2.0 |

Safe areas (notch / Dynamic Island, home indicator, Android system bars) are
handled in QML only: `qml/mobile/main.qml` reads the `SafeArea` attached
property of the window (`safeTop/Bottom/Left/Right`, in item units, updated on
rotation and iPad window resizing). Pages and their text stay inside; the top
inset is painted in the header colour and the menu bar (`MainMenuBar`
`bottomInset`) and bottom sheets (`BottomSheet`) run their background under the
bottom inset. On iOS the window sets `Qt.ExpandedClientAreaHint` (without it
Qt 6.9+ stops the window at the safe area and iOS shows black bands). Android
15+ (targetSdk 35+) is always edge to edge; there the app also requests the
dark colour scheme so that the transparent status bar gets light icons.
Android 14 and older keep opaque system bars outside the window: the insets are
0.

## 5. Networking & lifecycle differences vs. desktop

`Application::createQmlApp()` connects to `HardwareUtils` lifecycle signals
specifically used on mobile:

- `applicationWillResignActive` (iOS only) — drops the connection with
  `CalaosConnection::pause()` and resets the start-option flag. This is not a
  logout: the policy remembers whether a session was wanted, and refuses every
  automatic reconnection (network back, retry timer) while in the background.
- `applicationBecomeActive` (iOS only) — `unpause()`, then re-issues `login()`
  from the saved keychain credentials, **unless the user logged out**
  (`sessionClosedByUser()`): an explicit logout survives the app going to the
  background and back.
- `networkStatusChanged` (iOS Reachability, Android `QNetworkInformation`) —
  `suspend()` when the link is lost, `resume()` when it is back. `resume()`
  only reconnects a session that is wanted: never after a logout or the login
  cancel button, never while paused (FIX-7).
- Reconnection itself (backoff, single retry timer) is the same on all
  variants, see `ReconnectPolicy` in
  [CalaosConnection.h](../src/CalaosConnection.h). While it is not connected
  the mobile UI shows the login screen
  (`stackView.pop(null, StackView.Immediate)`).

WebSocket / HTTP fallback logic in
[CalaosConnection](../src/CalaosConnection.cpp) is identical on both variants
(`wss://<host>/api/v3` preferred; HTTP `https://<host>/api.php` fallback).

## 6. Mobile QML structure

Top-level: [qml/mobile/main.qml](../qml/mobile/main.qml). It hosts a `StackView`,
a `MainMenuBar` and listens to `Keys.Key_Back`/`Key_Backspace` for hardware-back
support on Android.

Views in [qml/mobile/](../qml/mobile):

- `LoginView.qml`, `MainMenuBar.qml`, `MainMenuButton.qml`, `MediaMenuView.qml`,
  `MediaMenuItem.qml`, `ViewHeader.qml`, `ListGroupHeader.qml`,
  `PullRefreshHeader.qml` — navigation chrome.
- `RoomListView.qml`, `RoomDetailView.qml`, `ScenarioView.qml`,
  `SensorDetailView.qml` — home automation views.
- `FavoritesListView.qml`, `FavoritesAddView.qml`, `FavoritesEditView.qml` —
  per-user favorites.
- `CameraListView.qml`, `CameraSingleView.qml`, `IconCamera.qml` — cameras.
- `MusicListView.qml`, `IconMusic.qml`, `IconMusicPlayer.qml` — audio.
- `EventLogView.qml`, `PushEventView.qml` — event log + push notification deep
  link viewer.
- `SettingsView.qml`, `ColorPickerRGBView.qml`, `ColorPickerView.qml`,
  `CalaosTextField.qml`, `ButtonLogin.qml`, `ButtonFooter.qml`,
  `StyleButtonDefault.qml` — settings & utilities.

The shared IO components (`IOLight`, `IOShutter`, …) come from
[qml/SharedComponents/](../qml/SharedComponents).

## 7. QuickFlux actions

Shared with desktop. Definitions live in [qml/quickflux/](../qml/quickflux):

```
ActionTypes.qml      // KeyTable singleton listing every action
ActionCreator.qml    // wrappers that dispatch the action
```

Mobile screens listen to the dispatcher (`QFAppDispatcher`) for events such as
`openCameraSingleView`, `openEventPushViewerUuid`, `showNotificationMsg`,
`showSensorDetails`. Push notifications received while the app is running are
forwarded through `HardwareUtils::pushNotifReceived(uuid)` and ultimately become
QuickFlux dispatches.

## 8. Authentication & secure storage

- Credentials are kept in the platform keychain via
  `HardwareUtils::saveAuthKeychain` / `loadAuthKeychain` /
  `resetAuthKeychain`.
- iOS: implementation in
  [ios/SimpleKeychain](../ios/SimpleKeychain) used by
  [ios/HardwareUtils.mm](../ios/HardwareUtils.mm). Keychain group is
  `fr.calaos.CalaosMobile`.
- Android: native dialogs from
  [android/src/fr/calaos/calaoshome/HardwareUtils.java](../android/src/fr/calaos/calaoshome/HardwareUtils.java);
  credentials and server host in
  [CredentialStore.java](../android/src/fr/calaos/calaoshome/CredentialStore.java)
  (SharedPreferences encrypted with an Android Keystore AES-GCM key, excluded
  from backup), written/read by the `HardwareUtilsAndroid` overrides, so the
  push picture worker can use them without Qt running. Credentials saved in
  `QSettings` by earlier versions are migrated at the first start and the
  clear-text copy removed. See [android.md](android.md) section 6.4.

## 9. Push notifications

Both platforms wire push delivery into a single QML signal: the
`Application::pushNotificationReceived(uuid)` slot triggers a QuickFlux
`openEventPushViewerUuid` dispatch (mobile only). The transport is
platform-specific:

- **iOS**: APNs + a Notification Service Extension (`PushNotif.appex`) that
  adds the badge and the camera picture (credentials from the shared
  keychain) and stashes admin codes (section 9.1). See [ios.md](ios.md).
- **Android**: Firebase Cloud Messaging via the Firebase Java SDK
  (`CalaosMessagingService`), data-only messages requested with
  `push_format: "data"` in `register_push` (`PushFormat` in
  `CalaosConnection.h`), camera picture attached by an expedited WorkManager
  job. See [android.md](android.md) section 6.

Shared rules (in [src/HardwareUtils.cpp](../src/HardwareUtils.cpp) and
[src/CalaosConnection.cpp](../src/CalaosConnection.cpp)):

- **Push token**: platform code reports it with `HardwareUtils::setDeviceToken()`,
  which emits `deviceTokenChanged()` when the value changes. `CalaosConnection`
  sends `register_push` once the login is accepted (websocket or HTTP), and
  again when the token changes while the session is up (FCM rotation, APNs
  token arriving after the login). The same token is never sent twice in one
  session (`PushRegistration` in `CalaosConnection.h`). Over HTTP the request
  carries `cn_user`/`cn_pass` like every api.php call. It also carries
  `device_name` (`HardwareUtils::effectiveDeviceName()`), which the admin
  uses to pick the approver phone in the web app: the name typed in
  Settings → "Name of this device" (config option `device_name`, sanitized:
  control characters and line breaks become spaces, bidi controls and
  invisible characters dropped, whitespace collapsed, trimmed, at most 64 UTF-16 code units cut on a grapheme boundary; empty =
  no override), else `HardwareUtils::getDeviceName()` (iOS `UIDevice.name`,
  the generic "iPhone" since iOS 16; Android `Settings.Global.DEVICE_NAME`,
  else manufacturer + model). A rename in the app emits `deviceNameChanged()`
  and is sent right away over the current transport when a session and a
  token exist, else with the next login; `PushRegistration` keys on the
  token and the name, so an unchanged name is never resent. "Reset all data"
  drops the override. A rename in the system settings only reaches the
  server with the next `register_push` (next login or token rotation). The token is a
  per-device credential: it is only ever logged truncated
  (`HardwareUtils::describeToken()`), and redacted from the HTTP debug dumps.
- **Notification tap**: platform code calls
  `HardwareUtils::handleNotificationOpened(uuid)`. Logged in → opened right away
  through `pushNotifReceived(uuid)`. Not logged in (cold start, app in the
  background, reconnection in progress) → kept, the latest tap wins, and
  `Application::homeLoaded()` opens it once at the next login
  (`takePendingNotif()`, then `DeferredNotifGate` in `HardwareUtils.h`: a
  session that drops before the event is shown hands it back with
  `restorePendingNotif()`, and a delay armed by an earlier login never opens
  anything).
- **Explicit logout** (Settings → Logout, `Application::userLogout()`, and
  Settings → Reset all data, `Application::resetAllData()`):
  after `logout()` hands back a tap taken but not opened yet,
  `HardwareUtils::forgetPendingUserActions()` drops the deferred tap and the
  iOS Quick Action start option, so neither fires (possibly stale, possibly
  for another home) at the next manual login (FIX-7). A lost connection, the
  login cancel button (plain `logout()`) and the iOS resign-active pause keep
  them for the next login, as above.

### 9.1 Encrypted admin code

The admin 2FA code is no longer in the notification text: the box seals it
with this phone's **push key** and the app decrypts and shows it
(contract: calaos_base `docs/08_http_api.md`, "Code push chiffré";
code: [src/AdminCode.h](../src/AdminCode.h),
[qml/mobile/AdminCodeSheet.qml](../qml/mobile/AdminCodeSheet.qml)).

- **Push key**: 32 bytes from the platform CSPRNG, created at the first start,
  kept in the platform key store (iOS keychain item `calaos-push-key`,
  service `CalaosMobile.push`, this-device-only, credentials' access group;
  Android `CredentialStore.pushKey()`, Keystore-encrypted in `calaos_push.xml`),
  never in `QSettings`, never logged. A new key is only created when the store
  says the old one is really gone: a store that cannot answer now (iOS before
  the first unlock, a transient Keystore error) leaves the key empty and
  `HardwareUtils::loadPushKey()` is called again on the next foreground or
  admin code notification (an approver's key is frozen on its box, replacing
  it would lock the phone out). Reported with `HardwareUtils::setPushKey()`;
  `register_push` carries it as `push_key` (64 lowercase hex, WebSocket
  `data` and HTTP root alike) **over TLS only** (`wss://`, `https://`; never
  on a plain `ws://`/`http://` connection) and is sent again when it changes
  (`PushRegistration` keys on token, name and key). Redacted from the HTTP
  debug dump. "Reset all data" keeps it: the box freezes the
  key of an approver phone. No key (desktop, key store failure) → no
  `push_key`, the box keeps pushing the legacy text, which still shows as
  before.
- **Reception**, no cryptography: the payload (`admin_code_id`, `_action`,
  `_expires`, `_nonce`, `_ct`, `_tag`) is stashed as received with the
  reception time by the phone's wall clock (`received_at`, ms) and boot clock
  (`received_mono`: Android `elapsedRealtime()`, Darwin `CLOCK_MONOTONIC`;
  both count during sleep and are shared by all processes). Android: the
  messaging service, in `CredentialStore` (`admin_codes`). iOS: the
  notification extension, one file `admin_codes/<code_id>.json` in the App
  Group container (both platforms drop entries more than 10 minutes old and
  keep at most 16); the app stashes it itself (date of the notification,
  wall clock only) when it sees one the extension did not (presented,
  tapped, or still in the notification center at foreground). The
  notification only shows the generic notice; a swiped one loses nothing.
- **Display**: `AdminCodeModel` reads the stash once the UI is up, on every
  `HardwareUtils::adminCodeNotified()` (push arrived while running, or tapped)
  and on every return to the foreground. Not tied to the login: the phone
  never needs the box to read a code. Per entry: shapes checked first
  (action in the table, strict `expires`, lowercase hex lengths), then the
  tag recomputed from the strings as received and compared in constant time,
  then `code = ct XOR ks[0..6)`. A rejected entry is dropped (one warning
  without values) and the sheet says "A code arrived but could not be read on
  this phone", so an approver knows its key must be recovered. Without a key
  yet, nothing is decrypted and the stash is left untouched. The sheet lists the codes newest first with the
  purpose, the 6 digits, a 1 s countdown and a draining bar, and Copy
  (iOS: pasteboard item expiring after 60 s; Android: `ClipData` flagged
  sensitive; desktop test build: `QClipboard`; everywhere but iOS cleared
  after 60 s if it still holds the code). While the sheet is open Android sets
  `FLAG_SECURE`; the digits are hidden whenever the app is not active (best
  effort against the iOS app switcher snapshot). A code is shown 60 s from its
  reception on the phone, never from `expires` (box clock; authenticated,
  not compared): the longer of the wall and boot clock ages counts, a boot
  clock below the stamp (reboot) or a wall clock set back drops it. At expiry
  the digits become "Expired". Expired and rejected entries leave the stash
  right away, shown ones when the sheet is closed (their notification is
  withdrawn too). Never logged: key, nonce, ct, tag, code.
- iOS foreground: a push with an admin code shows no banner
  (`foregroundPushPresentation()`), the sheet opens instead.
- Desktop/kiosk: no key, no `push_key`, the model is not created.

## 10. Internationalisation

Same translation pipeline as desktop ([lang/](../lang)). The mobile app respects
`HardwareUtils::getConfigOption("lang")` and sets the matching QTranslator at
startup (`Application::setupLanguage()`).

## 11. Versioning

- Android: derived from [VERSION.txt](../VERSION.txt) by
  [cmake/Versioning.cmake](../cmake/Versioning.cmake) (`versionName` = semver,
  `versionCode` = `MAJOR*10000 + MINOR*100 + PATCH`) and injected into the
  manifest by Qt; the manifest only holds placeholders.
- iOS: `MARKETING_VERSION` and `CURRENT_PROJECT_VERSION` are Xcode build
  settings (set in the Xcode project that qmake regenerates). The displayed
  bundle name is `CalaosHome`, executable `CalaosMobile`, bundle id
  `fr.calaos.CalaosMobile` (see [ios/AppInfo.plist](../ios/AppInfo.plist) and
  [ios/ios.pri](../ios/ios.pri)).
- C++ side: `PKG_VERSION_STR` from [src/version.h](../src/version.h) — overridden
  by packaging.

## 12. Things an LLM should remember

1. Any new shared C++ source goes into [calaos.pri](../calaos.pri); platform
   sources go into [cmake/Android.cmake](../cmake/Android.cmake) (Android, CMake
   only) / [ios/ios.pri](../ios/ios.pri) (iOS); never declare iOS or Android
   sources in [mobile.pro](../mobile.pro) directly. New shared sources are
   also listed in [cmake/CalaosHomeSources.cmake](../cmake/CalaosHomeSources.cmake)
   (`tools/check_sources.sh`).
2. The `QAPP` macro abstracts `QGuiApplication` vs. `QApplication`. Don’t add
   widgets-only APIs without guarding them with `#ifndef CALAOS_MOBILE` or
   pulling `widgets` only in the desktop fallback.
3. Mobile **does not** ship the desktop-only widgets, OS install wizard,
   `CalaosOsAPI`, `ScreenManager`, `WeatherInfo`, `ControlPanelModel`,
   `UsbDisk`, `CalaosWidgetModel`, `UserInfoModel`. Code referencing them must
   live behind `#ifdef CALAOS_DESKTOP`.
4. The mobile login screen is the entry; the app expects to be backgrounded
   often — keep critical state in models that survive logout/reload.
5. The QuickFlux dispatcher is the canonical way to cross UI boundaries from
   C++; prefer it over direct QML method calls.
6. iOS- and Android-specific files **must not** be referenced from the desktop
   build path — keep them isolated under [ios/](../ios) and
   [android/](../android).
