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
- Exception, [`ResumePolicy`](../src/ResumePolicy.h): resign-active is also
  sent when the password manager (1Password, iCloud Keychain) takes over to
  fill the native password entry, and an app switch from the box updates
  page is the same case. When the native admin password entry is open or the
  updates page is shown, the session still drops but
  `Application::keepPageOnResume` tells `main.qml` to keep the page stack
  (no pop to the login screen, no push of the home page); the automatic login
  on return reconnects in place, and a failure of that login goes to the
  login screen as usual. With the native login entry open, no automatic login
  runs on return: the user logs in himself with the filled fields.
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
- `UpdatesView.qml` — box updates with the admin password (section 8.1).
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

### 8.1 Box updates (admin password)

Settings → « Box updates » opens [UpdatesView.qml](../qml/mobile/UpdatesView.qml):
check and install the box updates and reboot it, with the **admin** password
(distinct from `cn_user`/`cn_pass`), through calaos_server's admin relay on the
household WebSocket (contract: calaos_base `docs/08_http_api.md`,
« Administration »). No bootstrap, password change, approver or console from
the phone: those stay in the webapp.

- [`AdminRelay`](../src/AdminRelay.h): `admin_unlock {admin_pass}` and
  `admin {action, admin_session, params}`, correlated by `msg_id`, 5 s timeout
  (125 s for `update_check`); replies routed by `CalaosConnection`
  (`adminReplyReceived`, `adminEventReceived`), frames sent through its
  `AdminTransport` interface (WebSocket only). Gates, named after the webapp
  store: `idle`, `probing`, `locked`, `unlocked`, `https_required`,
  `admin_not_set`, `unavailable` (`admin_unavailable`), `too_old`
  (`daemon_too_old`), `unreachable` (`daemon_unreachable`, or an unexpected
  error on the probe), `old_server` (no answer at all on this socket).
- Admin session: memory only, never persisted or logged; 15 min absolute at
  most (`expires_in`, monotonic and wall clock, checked again when the app
  comes back to the foreground); dropped with the socket (logout, lost link,
  iOS background), on `bad_session`, on `admin_event admin_set/admin_reset`, and
  when the user trusts a new certificate key (`tlsTrustAccepted`). No admin
  frame goes out while a certificate question is pending.
- [`UpdateBackend`](../src/UpdateBackend.h) is the transport interface of the
  update calls: `CalaosOsAPI` implements it on the kiosk (daemon direct),
  [`AdminRelayBackend`](../src/AdminRelayBackend.h) on mobile (admin actions,
  `admin_event upgrade_progress`). [`AdminUpdates`](../src/AdminUpdates.h)
  (context property `boxUpdates`) is the screen's flow, mirroring the webapp's
  UpdatesPanel (phases `unknown`, `upToDate`, `available`, `upgrading`, `done`,
  `doneReboot`, `failed`), and feeds the shared update models
  (`UpdatePackagesModel`, `UpdateGroupsModel`, `UpdateProgressModel`,
  `CalaosOsProtocol` parsing).
- `update_status` is the probe and the source of truth after a reconnect; it
  works without a session (household view: progress figures, no package
  names). Package names and versions are only shown once unlocked, and are
  cleared when the session ends.
- Gate names against the webapp store (`stores/admin.ts`): `locked`,
  `unlocked`, `https_required`, `unavailable`, `unreachable`, `old_server`
  are the same; `admin_not_set` covers the webapp's `bootstrap` and
  `household` (no bootstrap from the phone); `too_old` is its `activating`;
  `idle`/`probing` are its `idle`/`probing`; `websocket_required` is
  mobile-only (logged in over the HTTP fallback, which has no `admin`
  message).
- The password never leaves on a socket that has not proved itself:
  `admin_unlock` is refused without sending while a certificate question is
  pending, over the HTTP fallback, before an admin answer on the current
  socket, and on a non-`wss` socket (`https_required`, the box reached as
  `http://…:5454`). Every socket loss resets the gate to `idle`: a screen
  from box A never collects a password for box B. Requests made with the
  session fail (`session_lost`) when it ends, so their answers cannot bring
  package data back.
- The password field is `TextInput.Password` with
  `Qt.ImhSensitiveData | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase`,
  never bound to a setting, cleared on submit, when leaving the page, when the
  gate leaves `locked` and when the app leaves the foreground. On iOS and Android the password is typed in a
  native dialog instead (`HardwareUtils::requestNativePassword()`, purpose
  `admin_unlock`; the login screen uses it for its password, purpose
  `login`): Qt's text fields get no password AutoFill on either platform
  (iOS: see [ios.md](ios.md) section 5; Android: Qt 6.12 routes every field
  through one proxy `QtEditText` without autofill hints or view structure).
  The dialog has a username field (`admin`, or the typed household user) and
  a password field marked for the password manager (`UITextContentTypePassword`,
  `View.AUTOFILL_HINT_PASSWORD`), answers once, wipes its fields, stores and
  logs nothing; the QML field stays where no native entry exists (desktop
  test build). An admin password that comes back while the socket is being
  reopened (after the password manager) is held by `AdminUpdates` in memory
  for 30 s at most and sent once the box has answered on the new socket, to
  the same host only; leaving the page, a timeout or any other gate drops it,
  and `AdminRelay` still refuses it without `wss`, before a probe answer or
  while a certificate question is pending.

**What is shared with the kiosk, and what is not.** Shared sources (both
builds): `UpdateBackend` (interface), `UpdateModel` (the three models),
`UpdateGrouping` (including `splitPackages()`, the calaos/groups split both
orchestrators use), `CalaosOsProtocol` (`<Status>`/`<Job>` parsing, `isIdleProgress`).
`CalaosOsAPI` implements `UpdateBackend` but stays desktop-only, and so does
`UpdateManager`: the kiosk flow was not moved onto the interface because it
depends on what only the daemon offers (installed package list, legacy
synchronous routes, the daemon's own `/events` socket with its `upgrade_job`
events, detection of a self-upgrading daemon, multi-POST group queues,
automatic checks with the update dialog). Moving it would have risked the
kiosk behaviour for no mobile gain. `AdminUpdates` is therefore a second
orchestrator over the shared models: its phase logic follows the webapp's
UpdatesPanel, and its model feeding mirrors `UpdateManager`. Twin code to
change together: `AdminUpdates::applyAvailable()` ↔
`UpdateManager::applyAvailable()` (model feeding around the shared split),
`AdminUpdates::applyProgress()` ↔ the package part of
`UpdateManager::applyStatusSnapshot()`, and the terminal/idle frame handling
(`AdminUpdates::held()`/`onProgressEvent()` ↔ `applyStatusSnapshot()`).
`AdminRelay`, `AdminRelayBackend` and `AdminUpdates` are compiled into the
kiosk too (shared sources) but never instantiated there.

## 9. Push notifications

Both platforms wire push delivery into a single QML signal: the
`Application::pushNotificationReceived(uuid)` slot triggers a QuickFlux
`openEventPushViewerUuid` dispatch (mobile only). The transport is
platform-specific:

- **iOS**: APNs + a Notification Service Extension (`PushNotif.appex`) that
  decrypts payloads using a shared keychain item. See [ios.md](ios.md).
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
   `UsbDisk`, `CalaosWidgetModel`, `UserInfoModel`, `UpdateManager`. Code
   referencing them must live behind `#ifdef CALAOS_DESKTOP`. The update
   models, `CalaosOsProtocol` and `UpdateBackend` are shared: mobile updates go
   through `AdminRelay` (section 8.1).
4. The mobile login screen is the entry; the app expects to be backgrounded
   often — keep critical state in models that survive logout/reload.
5. The QuickFlux dispatcher is the canonical way to cross UI boundaries from
   C++; prefer it over direct QML method calls.
6. iOS- and Android-specific files **must not** be referenced from the desktop
   build path — keep them isolated under [ios/](../ios) and
   [android/](../android).
