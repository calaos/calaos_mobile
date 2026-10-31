# Calaos Home — Desktop variant

> LLM-oriented reference for the **desktop / touchscreen kiosk** build of `calaos_home`.
> Companion docs: [mobile.md](mobile.md), [ios.md](ios.md), [android.md](android.md).

## 1. Purpose & target

The desktop build is the official **Calaos OS** touchscreen kiosk client. It runs full-screen on
a Linux machine (typically a Calaos OS appliance) under an X11 session managed by the
`ratpoison` window manager. It is intended for:

- Wall-mounted touchscreens controlling rooms, lights, shutters, scenarios, audio, cameras.
- System-level operations: network configuration, OS install/update, snapshot rollback,
  user management, screen suspend (DPMS).
- A user-facing desktop with **draggable widgets** (clock, sticky notes, weather).

The variant is selected at build time by the `CALAOS_DESKTOP` preprocessor define and the
[desktop.pro](../desktop.pro) qmake project file.

## 2. Build

### 2.1 Required Qt modules

From [desktop.pro](../desktop.pro):

```
QT += core widgets network qml core5compat
qtHaveModule(webenginequick): QT += webenginequick   # optional, defines HAVE_WEBENGINE
```

Linux dependencies (via `pkg-config`): `x11`, `xext`. C++ standard: C++11. Qt 6 is
required (uses `core5compat`, `QtWebEngineQuick`, `QJniObject`-free path, etc.).

### 2.2 Build commands

```bash
qmake desktop.pro PREFIX=/usr
make -j$(nproc)
sudo make install        # installs binary `calaos_home` to $PREFIX/bin
```

`PREFIX` defaults to `/usr/local`. The macro `ETC_DIR=$PREFIX/etc` is propagated to C++.
`DEFINES += QT_MESSAGELOGCONTEXT` enables file/line in log output (used by
`Common::installMessageOutputHandler`).

### 2.3 Runtime environment

`src/main.cpp` sets `QT_IM_MODULE=qtvirtualkeyboard` before the `Application` instance is
created, so the on-screen virtual keyboard is automatically used.
`HardwareUtils::Instance()` is created **before** `QApplication` and
`QtWebEngineQuick::initialize()` is called when WebEngine is present.

The class hierarchy collapses through a `QAPP` macro defined in [src/Application.h](../src/Application.h):
on desktop, `QAPP = QApplication` (full widgets stack, needed by virtual keyboard / dialogs).

## 3. Resources & QML entry point

QML root: [qml/desktop/main.qml](../qml/desktop/main.qml). The engine import path is
`qrc:/qml/` and the QML module name registered for C++ types is `Calaos` (version 1.0).

Resource files compiled in (from [desktop.pro](../desktop.pro)):

- [qml_desktop.qrc](../qml_desktop.qrc) — desktop QML files
- [qml_shared.qrc](../qml_shared.qrc) — `SharedComponents` module (IO widgets, theme)
- [img_desktop.qrc](../img_desktop.qrc), [fonts_desktop.qrc](../fonts_desktop.qrc)
- [widgets.qrc](../widgets.qrc) — desktop widget pack (Clock/Note/Weather)
- [lang.qrc](../lang.qrc) — compiled `.qm` translations
- `qml/desktop/keyboard_style/calaos_style.qrc` — virtual keyboard skin

Window default size: `1024 × 768 dp` scaled by `calaosApp.density`. Pass `--force-hdpi`
to double the density at startup (see [Application.cpp](../src/Application.cpp)).

## 4. Desktop-only C++ sources

These are added by [desktop.pro](../desktop.pro) on top of the shared list in
[calaos.pri](../calaos.pri):

| File | Role |
|---|---|
| [src/HardwareUtils_desktop.cpp](../src/HardwareUtils_desktop.cpp) | Desktop platform impl (settings via `QSettings`, no keychain, no push notif). |
| [src/CalaosOsAPI.cpp](../src/CalaosOsAPI.cpp) | REST client for Calaos OS local daemon: `getFsStatus`, `rebootMachine`, `restartApp`, `rollbackSnapshot`, `listInstallDevices`, `startInstallation`, `getNetworkInterfaces`, `getSystemInfo`, `configureNetwork`, `getBoxTime`, `setBoxTime`, `getTimezones`, `restartService`. Singleton. |
| [src/BoxTimeManager.cpp](../src/BoxTimeManager.cpp), [src/BoxTime.cpp](../src/BoxTime.cpp) | Date and time of the box for `ConfigTimeView.qml` (`boxTime`): reads and writes through `CalaosOsAPI` on `/api/v2/system/time` only, restart after a time zone change. |
| [src/CalaosWidget.cpp](../src/CalaosWidget.cpp), [src/CalaosWidgetModel.cpp](../src/CalaosWidgetModel.cpp) | Desktop widget system — load/save widget instances and positions to disk. |
| [src/ControlPanelModel.cpp](../src/ControlPanelModel.cpp) | Model backing the configuration / control panel screens. |
| [src/OSInstaller.cpp](../src/OSInstaller.cpp) | Wizard for installing Calaos OS onto a USB / disk target (only enabled when running from a live medium). |
| [src/WeatherInfo.cpp](../src/WeatherInfo.cpp) | Weather data model used by the Weather widget. |
| [src/XUtils.cpp](../src/XUtils.cpp) | X11 helpers (links against `x11`, `xext`). |
| [src/ScreenManager.cpp](../src/ScreenManager.cpp) | DPMS singleton: suspend/wake the screen, idle timer. |
| [src/UserInfoModel.cpp](../src/UserInfoModel.cpp) | Per-user info (singleton, persisted). |
| [src/UsbDisk.cpp](../src/UsbDisk.cpp) | USB disk enumeration for OS installation (uses `3rd_party/drivelist_linux/linuxdrivelist.cpp`). |
| [src/AsyncJobs.cpp](../src/AsyncJobs.cpp) | Async job chaining utility used by `CalaosOsAPI`. |
| [src/NetworkRequest.cpp](../src/NetworkRequest.cpp) | Low-level HTTP helper used by `CalaosOsAPI`. |
| [src/version.h](../src/version.h) | `PKG_VERSION_STR` (`"git"` in tree, overridden at packaging). |

The 3rd-party `drivelist` headers come from `3rd_party/drivelist/`; the Linux
implementation is compiled directly.

## 5. QML context properties (desktop only additions)

Set in [Application::createQmlApp()](../src/Application.cpp):

| Property | Type / source | Notes |
|---|---|---|
| `widgetsModel` | `CalaosWidgetModel::Instance()` | Singleton, loaded from disk at startup, saved on quit. |
| `screenManager` | `ScreenManager::Instance()` | DPMS / wake-up control. |
| `userInfoModel` | `UserInfoModel::Instance()` | |
| `controlPanelModel` | `ControlPanelModel*` | |
| `webEngineProfile` | `QQuickWebEngineProfile*` | Only when `HAVE_WEBENGINE`. Persistent cookies + disk cache. |
| `usbDiskModel`, `osInstaller` | only when `Application::hasInstall` is true | Detected via `/.calaos-live`, `/run/calaos/calaos-live`, `C:/temp/calaos/calaos-live`. |
| `bookmarkModel` | `BookmarkModel*` | Used by the embedded web browser (WebEngine). |

Shared (also exposed on mobile): `calaosApp`, `homeModel`, `scenarioModel`, `lightOnModel`,
`audioModel`, `favoritesModel`, `favoritesHomeModel`, `cameraModel`, `langModel`,
`eventLogModel`.

## 6. Desktop QML structure

Top-level: [qml/desktop/main.qml](../qml/desktop/main.qml) hosts a `MainMenu` and a
`StackView`. Routing is performed with `handleSubitemClick(itemId)` mapping string ids to
component instances.

Key views in [qml/desktop/](../qml/desktop):

- `HomeView.qml`, `DesktopView.qml`, `HomeboardListView.qml` — landing screens and
  draggable desktop with widgets.
- `RoomDetailView.qml`, `ScenarioView.qml` — IO and scenario controls.
- `MediaView.qml`, `MusicListView.qml`, `AudioPlayer.qml`, `SpotifyView.qml`,
  `MediaWebView.qml`, `WebBrowserMenu*.qml` — media and embedded browser.
- `CameraListView.qml`, `CameraSingleView.qml`, `CameraCommandItem.qml`.
- `ConfigView.qml` + `ConfigNetworkView.qml`, `ConfigScreenView.qml`,
  `ConfigL18nView.qml`, `ConfigTimeView.qml`, `ConfigUserInfoView.qml`,
  `ConfigUpdateView.qml`, `ConfigInstallView.qml` — system / OS configuration tabs.
- Dialogs: `Dialog*.qml` (keyboard, RGB color picker, reboot, recovery boot,
  add-widget, sensor details, bookmarks, network interface).
- `ScreenSuspend.qml`, `IconScreensaver.qml`, `IconLockScreen.qml` — DPMS / idle UI.
- `OverlayLayer.qml`, `OverlayView.qml`, `Notification.qml`, `PopupBase.qml`,
  `BasePopupView.qml` — overlay infrastructure.
- `keyboard_style/` — `QtVirtualKeyboard` style override.

The shared IO components from `qml/SharedComponents/` (`IOLight`, `IOShutter`, etc.) are
imported via `import SharedComponents`.

## 7. Desktop widget system

- Widget assets live in [widgets/](../widgets) (`Clock/`, `Note/`, `Weather/`) and the
  manifest is [widgets/calaos_widgets.json](../widgets/calaos_widgets.json).
- Compiled into the binary through [widgets.qrc](../widgets.qrc).
- C++ side: `CalaosWidget` represents a widget instance, `CalaosWidgetModel` is a
  singleton model that persists positions and instances to disk
  (`saveToDisk()` is called in `Application::~Application()`).
- New widgets: add a `<name>/` directory with at least one QML entry and register it
  in `calaos_widgets.json`, then add files to `widgets.qrc`.

## 8. Calaos OS integration

### 8.1 `CalaosOsAPI`

A REST wrapper (HTTP, on `127.0.0.1` Calaos OS daemon) for system operations
(see section 4). It uses `NetworkRequest` and `AsyncJobs`. All calls take a
`std::function` callback receiving `(bool success, const QJsonValue &res)` or just
`bool success` for fire-and-forget calls.

### 8.2 Snapshot rollback

`Application::createQmlApp()` calls `CalaosOsAPI::getFsStatus()` and inspects the first
filesystem entry: if `target == "/"`, `source == "rootfs"`, `fstype == "overlay"`, the
property `isSnapshotBoot` becomes `true`. The QML layer can then offer the recovery
dialog (`DialogRecoveryBoot.qml`).

### 8.3 OS installation mode

Detected by `Application::hasInstall` when any of these files exist:
`/.calaos-live`, `/run/calaos/calaos-live`, `C:/temp/calaos/calaos-live`.
When true, the QML view `ConfigInstallView.qml` is wired up, `usbDiskModel` and
`osInstaller` are created, and the wizard becomes available.

### 8.4 Auto-reconnect

On WebSocket disconnect the desktop variant displays a translated "Network error"
alert and schedules a 1 s reconnect attempt — unlike mobile, which logs out fully on
backgrounding.

## 9. Settings & credentials

- Persisted via `QSettings` (`Calaos / CalaosHome`).
- On desktop there is **no platform keychain** — `HardwareUtils_desktop` stores the
  credentials inside `QSettings`. Treat them as locally trusted.
- Settings can be locked behind a password with `Application::unlockSettings()` /
  `lockSettings()` (a `QTimer` re-locks them automatically).
- A "demo mode" connects to `demo.calaos.fr` with `demo / demo` (see
  `Common::getDemoUser/Pass/Host`).

## 10. Internationalisation

- Source / compiled translations live in [lang/](../lang): `calaos_fr.ts/.qm`,
  `calaos_de.ts/.qm`. English is the built-in fallback.
- `Application::setLanguage(code)` sets the current locale via the embedded
  `QTranslator` and persists it through `HardwareUtils::setConfigOption("lang", code)`.
- `LangModel` is exposed to QML as `langModel` and drives `ConfigL18nView.qml`.
- Helper Go program: [lang/auto_translate.go](../lang/auto_translate.go) — bulk
  translates strings using an external API (see template
  `lang/auto_translate.conf_tmpl`).

## 11. Packaging & CI

The desktop build is packaged as an Arch Linux package (`calaos-home`) for Calaos OS
and a Debian package via downstream repos. Workflow files in
[.github/workflows/](../.github/workflows):

| Workflow | Trigger | Effect |
|---|---|---|
| [build.yml](../.github/workflows/build.yml) | push to `master` | Dispatches `build_pkg` to `calaos/calaos-build`. |
| [build_dev.yml](../.github/workflows/build_dev.yml) | manual | Dev package with explicit version tag. |
| [build_release.yml](../.github/workflows/build_release.yml) | manual | Same as dev for the stable repo. |
| [build_docker_dev.yml](../.github/workflows/build_docker_dev.yml) | push to `master` | Builds Docker image, pushes `:dev` tag to `ghcr.io/calaos/calaos_home`, dispatches deb build. |
| [build_docker_release.yml](../.github/workflows/build_docker_release.yml) | manual (major/minor/patch) | Same with `:latest` tag. |

The Docker image is built from [docker/Dockerfile](../docker/Dockerfile) (Arch Linux
based, two-stage). It produces a runtime image with `xorg`, `ratpoison`,
`xinput_calibrator` and the `calaos-home` package. Entrypoint is
[docker/start.sh](../docker/start.sh) which runs `startx` with
[docker/xinitrc](../docker/xinitrc); the `xinitrc` launches `ratpoison` and exec'es
`calaos_home` with `QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox`.

## 12. Things an LLM should remember

1. Any new C++ source must be added under the desktop-only block of
   [desktop.pro](../desktop.pro) **or** under [calaos.pri](../calaos.pri) if shared
   with mobile. Headers behind `#ifdef CALAOS_DESKTOP` should never be included from
   `calaos.pri`-listed sources.
2. New QML/asset files must be referenced inside the matching `.qrc`
   ([qml_desktop.qrc](../qml_desktop.qrc), [qml_shared.qrc](../qml_shared.qrc),
   [img_desktop.qrc](../img_desktop.qrc), [fonts_desktop.qrc](../fonts_desktop.qrc),
   [widgets.qrc](../widgets.qrc)) — otherwise they are invisible at runtime.
3. New shared QML components must also be declared in
   [qml/SharedComponents/qmldir](../qml/SharedComponents/qmldir).
4. `IOBase` is the canonical wrapper for **any** home automation point. Do not
   duplicate state — extend `IOBase` and the matching QML widget under
   `qml/SharedComponents/`.
5. `WebEngine` is **optional**. Always guard usage with `#ifdef HAVE_WEBENGINE` (C++)
   and `calaosApp.hasWebEngine` (QML).
6. There is no automated test suite. Validate changes by building both
   `desktop.pro` and `mobile.pro`.
7. Do not modify `3rd_party/`.
