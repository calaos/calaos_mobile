# Calaos Home — iOS build & deploy

> LLM-oriented reference for the **iOS** build of `CalaosMobile`.
> See [mobile.md](mobile.md) for the cross-platform mobile architecture.

## 1. Identifiers

| Setting | Value |
|---|---|
| Bundle id (main app) | `fr.calaos.CalaosMobile` |
| Bundle id (push extension) | `fr.calaos.CalaosMobile.PushNotif` |
| Display name | `CalaosHome` |
| Executable | `CalaosMobile` |
| Apple Development Team id | `9WYC46992U` |
| Keychain Sharing group | `fr.calaos.CalaosMobile` |
| App Group | `group.fr.calaos.CalaosMobile` |
| Min iOS | 18.0 (CMake build; floor of the Qt 6.12 iOS libraries) |

These are pinned in [ios/ios.pri](../ios/ios.pri) and
[ios/AppInfo.plist](../ios/AppInfo.plist):

```qmake
CALAOS_BUNDLE.value = fr.calaos.CalaosMobile          # PRODUCT_BUNDLE_IDENTIFIER
CALAOSDEVELOPMENT_TEAM.value = 9WYC46992U             # DEVELOPMENT_TEAM
PUSH_ENTITLEMENTS.value = $$PWD/pushnotifications.entitlements
Q_ENABLE_BITCODE.value = NO
```

## 2. qmake build

### 2.1 Sources & frameworks

[ios/ios.pri](../ios/ios.pri) declares (excerpt):

```qmake
TARGET = CalaosMobile
QT += gui-private

LIBS += -framework SystemConfiguration -framework UserNotifications

OBJECTIVE_SOURCES += ios/HardwareUtils.mm \
                     ios/Reachability.m \
                     ios/AlertPrompt.m \
                     ios/SimpleKeychain/A0SimpleKeychain.m \
                     ios/SimpleKeychain/A0SimpleKeychain+KeyPair.m
HEADERS         += ios/HardwareUtils_iOS.h
QMAKE_INFO_PLIST = ios/AppInfo.plist
QMAKE_ASSET_CATALOGS = ios/ressources.xcassets
QMAKE_BUNDLE_DATA += plugins splash   # adds PushNotif.appex + Splash.storyboard
```

The Reachability sources monitor the network through `SystemConfiguration` and
emit `HardwareUtils::networkStatusChanged()`.

### 2.2 Generating the Xcode project

```bash
qmake mobile.pro -spec macx-ios-clang CONFIG+=iphoneos    # or +iphonesimulator
open CalaosMobile.xcodeproj
```

This regenerates the Xcode project. **Every time** `qmake` is rerun (or the
build folder is cleaned) you must redo the manual Xcode-side steps in section 3
because the qmake-generated project does not preserve them. (`xcodeproj` could
script this but is incompatible with qmake's project format — see
[ios/readme.md](../ios/readme.md).)

## 3. One-time Xcode setup (after every qmake run)

Open the generated project in Xcode and configure these items.

### 3.1 Capabilities (main `CalaosMobile` target)

- **Push Notifications** — required for APNs.
- **Keychain Sharing** with group `fr.calaos.CalaosMobile`.
- **App Groups** with group `group.fr.calaos.CalaosMobile`.

The entitlements file [ios/pushnotifications.entitlements](../ios/pushnotifications.entitlements)
is wired in via `CODE_SIGN_ENTITLEMENTS` (see [ios/ios.pri](../ios/ios.pri)).

### 3.2 Notification Service Extension (PushNotif)

The repository ships [ios/PushNotif/](../ios/PushNotif) but the **target itself**
must be created from Xcode each time the project is regenerated. Steps from
[ios/readme.md](../ios/readme.md):

1. *File → New → Target → Notification Service Extension*.
2. Product Name: `PushNotif`. Language: Obj-C. Team: as the main app.
3. In the new target enable Capabilities:
   - Keychain Sharing → group `fr.calaos.CalaosMobile`
   - App Groups → group `group.fr.calaos.CalaosMobile`
4. Replace the auto-generated `PushNotif/` directory inside the build folder
   with a symlink to the source one:
   ```bash
   cd build-mobile-Qt_*_for_iOS-Debug
   rm -rf PushNotif
   ln -s ../calaos_mobile/ios/PushNotif .
   ```
5. In the PushNotif target → Build Settings → *Other Linker Flags*: delete every
   Qt-specific argument (the extension must not link Qt).
6. Build Phases → Compile Sources, add:
   - `A0SimpleKeychain.m`
   - `A0SimpleKeychain+KeyPair.m`
   - (`NotificationService.m` is added automatically.)
7. Verify that the extension's `Info.plist` is
   [ios/PushNotif/Info.plist](../ios/PushNotif/Info.plist) and the
   entitlements file is
   [ios/PushNotif/PushNotif.entitlements](../ios/PushNotif/PushNotif.entitlements).

The main app embeds the built `PushNotif.appex` via:

```qmake
plugins.path  = PlugIns
plugins.files = $$PWD/PushNotif.appex
QMAKE_BUNDLE_DATA += plugins
```

so the `.appex` must be present (built once by Xcode in the same configuration)
before re-running the main archive.

### 3.3 Launch screen

[ios/Splash.storyboard](../ios/Splash.storyboard) is registered through
`UILaunchStoryboardName` in [ios/AppInfo.plist](../ios/AppInfo.plist) and
embedded via `QMAKE_BUNDLE_DATA += splash`.

## 4. Push notification flow

| Step | Code |
|---|---|
| Request authorization, register with APNs | [ios/HardwareUtils.mm](../ios/HardwareUtils.mm) — `HardwareUtils_iOS::platformInit()` (called from `main()` once `QGuiApplication` exists, still inside the launch sequence) |
| Receive APNs token | `application:didRegisterForRemoteNotificationsWithDeviceToken:` installed at runtime on Qt's application delegate (`hookApplicationDelegate()`) → `HardwareUtils_iOS::handleRegisterNotif()` → `HardwareUtils::setDeviceToken()` |
| Token registered with `calaos_server` | `CalaosConnection` sends `register_push` at login, and again on `HardwareUtils::deviceTokenChanged()` while logged in (token arriving after the login) |
| Push received in background | `PushNotif.appex` (`ios/PushNotif/NotificationService.m`) — badge, camera picture (credentials from the shared keychain), and the admin code stash below; it never decrypts anything |
| Push key | `HardwareUtils_iOS::platformInit()`: keychain item `calaos-push-key` (32 bytes from `SecRandomCopyBytes`, `AfterFirstUnlockThisDeviceOnly`, service `CalaosMobile`, group `9WYC46992U.fr.calaos.CalaosMobile`), created at the first start → `HardwareUtils::setPushKey()` → `push_key` in `register_push` |
| Encrypted admin code arrives (payload `admin_code_*` in `data`, generic `message` as alert, `mutable-content`) | The extension writes the six strings as received plus `received_at` (wall clock, ms) and `received_mono` (`CLOCK_MONOTONIC`, ms) and `notif_id` to `admin_codes/<code_id>.json` in the App Group container (atomic write, never overwritten), then delivers the alert unchanged. In the foreground `willPresentNotification:` hands it to the app (no banner, the code sheet opens); a tap goes through `didReceiveNotificationResponse:`; on `UIApplicationDidBecomeActiveNotification` the app also looks at the delivered notifications. Each time `captureAdminCode()` stashes it if the extension did not (stamped with the notification's delivery date, wall clock only) and calls `HardwareUtils::handleAdminCodeNotified()`. Consumed or expired codes: file removed and delivered notification withdrawn (`removeAdminCodes()`). Decryption and display: `src/AdminCode.h`, see [mobile.md](mobile.md) section 9.1 |
| User taps notification while logged in | `userNotificationCenter:didReceiveNotificationResponse:` (app-owned `CalaosNotificationDelegate`) → `HardwareUtils::handleNotificationOpened(uuid)` → `pushNotifReceived(uuid)` → `Application::pushNotificationReceived(uuid)` → QuickFlux dispatch |
| User taps notification while not logged in (closed, background, reconnecting) | Same entry point; the uuid is kept and `Application::homeLoaded()` opens it once at the next login (`HardwareUtils::takePendingNotif()`, `DeferredNotifGate`: a session that drops before the event is shown hands it back to the next login) |
| Push **arrives** (no tap), app in the foreground or woken in the background | `application:didReceiveRemoteNotification:fetchCompletionHandler:` (runtime hook on Qt's application delegate) → `HardwareUtils::handleNotificationOpened(uuid)`: the event is opened as if tapped, right away when logged in, otherwise at the next login. Long-standing product behaviour, deliberately unchanged by FIX-3 |
| Push **arrives** with the app in the foreground | `userNotificationCenter:willPresentNotification:` (`CalaosNotificationDelegate`): an event push is also handed to the app (`pushNotifReceived(uuid)`, the event viewer opens when logged in). The system presentation comes from `foregroundPushPresentation()` in `src/HardwareUtils.h`: banner + list + sound, except an event push while logged in (list + sound, the viewer is already on screen) and an encrypted admin code (list + sound, the code sheet opens). Never the badge option. A legacy admin code text push (no `event_uuid`, no `admin_code_id`) is only visible through this banner |
| Push **displayed** with the app in the background or killed (icon badge, IOS-3) | `PushNotif.appex` increments `calaos.badge_count` in the App Group suite and sets `bestAttemptContent.badge` to it, first thing, so every path (picture or not, logged out, `serviceExtensionTimeWillExpire`) carries the badge. Needs `mutable-content: 1` in the payload (`calaos_server` sets it on every iOS push); without it the extension does not run and there is no badge |
| Badge cleared | `resetBadge()` in `ios/HardwareUtils.mm` sets the counter and the badge to 0 at launch (`platformInit()`), on `UIApplicationDidBecomeActiveNotification`, and when a push is presented in the foreground (`willPresentNotification:`, presented without the badge option). `resetAuthKeychain()` ("reset all data") removes the key |

Badge counter: the app and the extension are separate processes. The
extension's read-increment-write and the app's reset both take an exclusive
`flock()` on `calaos.badge_count.lock` in the App Group container, which also
covers concurrent extension requests. This is best effort: the extension waits
for the lock, the app (main thread) tries a few times for about 10 ms and then
goes on without it, the lock is skipped if the container cannot be opened, and
the defaults are written through cfprefsd whose ordering across processes is
not guaranteed. In those cases, or if the extension is killed after the
increment but before its content is delivered, the badge can be off by one
until the next launch, which resets it. The App Group suite therefore carries `calaos.hostname`,
`calaos.session_active`, `calaos.badge_count` and `calaos.tls_pins` (privacy
reason 1C8F.1 in both manifests).

TLS in the extension: its `NSURLSession` has a delegate. A certificate that
passes `SecTrustEvaluateWithError()` (public CA, matching name) is used as is;
otherwise the SHA-256 of the leaf's SubjectPublicKeyInfo must match
`calaos.tls_pins[host:port]`, the pins the user confirmed in the app
(`src/TlsPinning.h`, copied by `HardwareUtils_iOS::setTlsPins()`). A pinned
host must present its pinned key even when the system trusts the
certificate; trusting a CA-valid certificate in the app removes the pin of
that host (CM2b), which the system then validates alone. The extension cannot ask: no pin or another key skips the picture, the text
notification is still shown. ATS does not apply to IP addresses and `.local`
names; a self-signed box reached through a DNS name may additionally need an
ATS exception in the extension (to check on a device).

Background mode `remote-notification` is declared in
[ios/AppInfo.plist](../ios/AppInfo.plist) (`UIBackgroundModes`).

Application delegate (IOS-2): Qt owns the `UIApplication` delegate
(`QIOSApplicationDelegate`, private) and, since Qt 6.12, installs it only while
`QGuiApplication` is constructed (`UIApplicationMain()` starts with a placeholder,
`QIOSDynamicSceneDelegate`). Never put a category on Qt's delegate classes: it is
not sent `application:didFinishLaunchingWithOptions:` any more, and a category
silently replaces any method Qt adds later. The callbacks UIKit only sends to the
application or scene delegate are installed at runtime by `installHook()` (added
when missing, otherwise swizzled with Qt's implementation still called), see the
comment at the top of `ios/HardwareUtils.mm`.

## 5. UI / system integration

- Status bar style is `UIStatusBarStyleLightContent`,
  `UIViewControllerBasedStatusBarAppearance = false`
  (see [AppInfo.plist](../ios/AppInfo.plist)).
- Orientation: portrait only on iPhone (Face ID iPhones have no upside-down),
  all four on iPad.
- `CADisableMinimumFrameDurationOnPhone = true`: the app may render at 120 Hz on
  ProMotion iPhones.
- `LSRequiresIPhoneOS = true`.
- `NSPhotoLibraryUsageDescription` is kept: the app itself never reads the photo
  library, but Qt's iOS platform plugin (`libqios.a`, `qiosfiledialog.mm`) is always
  linked and references `PHPhotoLibrary`, which App Store upload checks flag when the
  purpose string is missing.
- App icon: a single 1024 px `AppIcon-1024.png` (no alpha) in
  [ressources.xcassets](../ios/ressources.xcassets); actool derives every size.
- Privacy manifests: [ios/PrivacyInfo.xcprivacy](../ios/PrivacyInfo.xcprivacy) (app) and
  [ios/PushNotif/PrivacyInfo.xcprivacy](../ios/PushNotif/PrivacyInfo.xcprivacy)
  (extension). Qt is linked statically, so the manifests inside the Qt frameworks do
  not reach the bundle: the app manifest also declares the required-reason APIs of the
  linked Qt code. Re-check after a Qt bump with
  `nm -u CalaosHome.app/CalaosHome | grep -E '^_(f|l)?stat(at)?$|statv?fs|getattrlist|mach_absolute_time|CFPreferences|NSUserDefaults'`
  and `strings -a CalaosHome.app/CalaosHome | grep -xE 'systemUptime|activeInputModes'`.
- Native alerts go through [ios/AlertPrompt.m](../ios/AlertPrompt.m).
- 3D Touch / Quick Actions: the app uses scenes (Qt does), so UIKit hands them to the
  window scene delegate: `windowScene:performActionForShortcutItem:completionHandler:`
  (app running) or the connection options of `scene:willConnectToSession:options:`
  (cold launch), both hooked on Qt's `QIOSWindowSceneDelegate` →
  `handlePerformActionForShortcutItem`,
  exposed to QML via `HardwareUtils::getStartOption()` and
  `HardwareUtils::setQuickLinks()`.

## 6. Keychain

- Library: vendored [`A0SimpleKeychain`](https://github.com/auth0/SimpleKeychain) under
  [ios/SimpleKeychain/](../ios/SimpleKeychain).
- Used by both the main app (credentials, push key) and the push extension
  (credentials for the camera picture), hence the shared
  `9WYC46992U.fr.calaos.CalaosMobile` keychain group. The push key item
  (`calaos-push-key`) is this-device-only and survives "Reset all data"
  (an approver phone's key is frozen on its box).
- Old [ios/KeychainItemWrapper.m](../ios/KeychainItemWrapper.m) is kept for legacy
  reasons; new code should use `A0SimpleKeychain`.

## 7. Build & deploy commands

```bash
# 1. Regenerate Xcode project
qmake mobile.pro -spec macx-ios-clang CONFIG+=iphoneos

# 2. Open & redo manual steps (section 3)
open CalaosMobile.xcodeproj

# 3. Build & archive from Xcode (Product → Archive)
#    or via xcodebuild:
xcodebuild -project CalaosMobile.xcodeproj \
           -scheme CalaosMobile \
           -configuration Release \
           -destination "generic/platform=iOS" \
           -allowProvisioningUpdates archive \
           -archivePath build/CalaosMobile.xcarchive

# 4. Export to .ipa (use an export options plist)
xcodebuild -exportArchive \
           -archivePath build/CalaosMobile.xcarchive \
           -exportOptionsPlist ios/ExportOptions.plist \
           -exportPath build/export
```

There is **no GitHub Actions workflow** that publishes the iOS app — releases
are uploaded manually to App Store Connect / TestFlight.

## 8. Things an LLM should remember

1. The Xcode project is **regenerated every time `qmake` runs**. Do not commit
   manual edits to it; commit them to [ios/](../ios) sources / `.pri` /
   entitlements / readme instead.
2. The PushNotif extension target is **not** part of qmake — only its sources
   are. After regeneration, recreate the target as documented (and consider
   automating it with a script if you do this often).
3. Bitcode is **disabled** (`ENABLE_BITCODE = NO`). Required because of the Qt
   static libraries and Firebase-style native dependencies are not bitcode-clean.
4. App Group + Keychain group must match between the main app and PushNotif
   extension; otherwise the extension cannot decrypt incoming pushes.
5. Increment `MARKETING_VERSION` and `CURRENT_PROJECT_VERSION` in Xcode
   *before* archiving; `AppInfo.plist` substitutes them via
   `$(MARKETING_VERSION)` / `$(CURRENT_PROJECT_VERSION)`.
6. New Obj-C / Obj-C++ sources must be declared in `OBJECTIVE_SOURCES` of
   [ios/ios.pri](../ios/ios.pri) — not in [calaos.pri](../calaos.pri).
7. iOS-specific assets go into [ios/ressources.xcassets](../ios/ressources.xcassets)
   and are wired through `QMAKE_ASSET_CATALOGS`.

## 9. CMake build (preferred)

The CMake path is the new canonical build. It generates a self-contained
Xcode project with both the main app and the `PushNotif` extension, and
requires no manual Xcode-side steps.

### 9.1 Prerequisites

- Xcode 15+ with command-line tools.
- Qt 6.6+ for iOS (tested with Qt 6.12): `qt-unified-installer` targets `ios_device` (+ `ios_simulator_arm64`); Qt <= 6.11 ships a single `ios` kit. Simulator builds need Qt >= 6.12 (`ios_simulator_arm64`); the script stops early otherwise.
- Apple Developer account with team id `9WYC46992U`.

### 9.2 Configure

```bash
cmake -S . -B build-ios -G Xcode \
      -DCMAKE_SYSTEM_NAME=iOS \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=18.0 \
      -DCMAKE_PREFIX_PATH=/path/to/Qt/6.12.0/ios_device \
      -DQT_HOST_PATH=/path/to/Qt/6.12.0/macos \
      -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=9WYC46992U
```

### 9.3 Build script, simulator

```bash
./tools/build_ios.sh Debug                 # device (build-ios/)
./tools/build_ios.sh Debug --simulator     # simulator, arm64 (build-ios-sim/), ad hoc signed
./tools/build_ios.sh --archive             # Release archive + export, see 9.4
```

The simulator build uses the Qt `ios_simulator_arm64` kit (Qt >= 6.12: the single
`ios` kit of Qt <= 6.11 has no arm64 simulator slice, the script stops early) and needs no team or
provisioning profile (ad hoc "Sign to Run Locally"; the entitlements are still
embedded, so the keychain group and App Group work in the simulator).

### 9.4 Build & archive to .ipa

```bash
cmake --build build-ios --config Release --target archive
```

This runs `xcodebuild archive` followed by `xcodebuild -exportArchive` using
[ios/ExportOptions.plist](../ios/ExportOptions.plist). The signed `.ipa` lands
in `build-ios/export/`.

### 9.5 What CMake handles automatically

- `PushNotif` extension target — defined in [ios/CMakeLists.txt](../ios/CMakeLists.txt),
  embedded into the main app via `XCODE_EMBED_APP_EXTENSIONS`.
- Signing entitlements, development team, bundle id, bitcode-off.
- Version numbers from [VERSION](../VERSION) (single source of truth).
- All sources, frameworks, asset catalog, launch storyboard, privacy manifests.

No manual Xcode steps needed after configure.

## Legacy qmake build

Sections 2–7 above document the legacy qmake path. It remains functional
during the transition period. New development should target the CMake path.
