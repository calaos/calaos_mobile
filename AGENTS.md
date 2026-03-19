# AGENTS.md - Calaos Home

## Project Overview

Calaos Home is the official client application for the [Calaos](https://calaos.fr) home automation system. It provides both a **desktop touchscreen interface** (for Calaos OS kiosks) and a **mobile app** (iOS/Android). The two variants share a common C++ backend and QML shared components, but have distinct UI layers.

- **Organization**: Calaos (`calaos.fr`)
- **License**: GPLv3+ with AppStore/Google Store exception (see `COPYING` and `COPYING.GPL`)
- **Main branch**: `master`
- **Docker image**: `ghcr.io/calaos/calaos_home`

## Technology Stack

| Layer | Technology |
|---|---|
| Language | C++11, QML/JavaScript |
| Framework | Qt 6 (QtQuick, QtWebSockets, QtNetwork, QtSvg) |
| Build system | qmake (`.pro` / `.pri` files) |
| State management (QML) | QuickFlux (Flux/Redux-like unidirectional data flow) |
| QML models | qt-qml-models (`QQmlObjectListModel`, `QQmlVariantListModel`) |
| Networking | WebSocket (primary) + HTTP fallback to `calaos_server` |
| Platforms | Android, iOS, Linux desktop (Calaos OS) |
| CI/CD | GitHub Actions, Docker (Arch Linux based) |
| i18n | Qt Linguist (`.ts` / `.qm` files), auto-translate tool in Go |

## Repository Structure

```
.
├── src/                    # C++ backend (models, networking, platform abstraction)
├── qml/
│   ├── desktop/            # QML UI for desktop/touchscreen variant
│   ├── mobile/             # QML UI for mobile (iOS/Android) variant
│   ├── quickflux/          # QuickFlux actions & dispatcher (shared)
│   └── SharedComponents/   # Reusable QML components (IO widgets, theming, etc.)
├── 3rd_party/
│   ├── quickflux/          # QuickFlux library
│   ├── qt-qml-models/      # QML model helpers
│   ├── drivelist/          # Drive listing (headers)
│   └── drivelist_linux/    # Linux drive listing implementation
├── android/                # Android platform code (JNI, Gradle, manifest)
├── ios/                    # iOS platform code (Obj-C, keychain, push notif)
├── docker/                 # Docker build for Calaos OS (Arch Linux)
├── fonts/                  # Font assets
├── img/                    # Image assets
├── lang/                   # Translations (.ts/.qm) + Go auto-translate tool
├── widgets/                # Desktop widgets (Clock, Note, Weather)
├── .github/workflows/      # CI/CD workflows
├── desktop.pro             # qmake project file for desktop variant
├── mobile.pro              # qmake project file for mobile variant
└── calaos.pri              # Shared qmake include (common sources & deps)
```

## Build Variants

The project has two distinct build targets controlled by preprocessor defines:

### Mobile (`CALAOS_MOBILE`)
- **Project file**: `mobile.pro`
- **Entry QML**: `qml/mobile/main.qml`
- **Qt modules**: `qml quick network gui websockets svg`
- **Targets**: Android (`Q_OS_ANDROID`), iOS (`Q_OS_IOS`), or desktop test mode
- **Uses**: `QGuiApplication` on mobile, `QApplication` on desktop

### Desktop (`CALAOS_DESKTOP`)
- **Project file**: `desktop.pro`
- **Entry QML**: `qml/desktop/main.qml`
- **Qt modules**: `core widgets network qml core5compat` + optional `webenginequick`
- **Target**: Linux (Calaos OS touchscreen kiosk)
- **Extra features**: OS installation, screen management (DPMS), widgets, weather, user management, control panel, network configuration, system monitoring
- **Linux deps**: `x11`, `xext` (via pkg-config)

### Build Commands

```bash
# Mobile
qmake mobile.pro
make

# Desktop
qmake desktop.pro PREFIX=/usr
make
make install
```

## Architecture

### Entry Point

`src/main.cpp` -> creates `Application` -> `HardwareUtils::Instance()` -> `app.createQmlApp()`

### Core C++ Classes (`src/`)

| File | Role |
|---|---|
| `Application.h/cpp` | Main application class. Inherits QGuiApplication (mobile) or QApplication (desktop). Bootstraps QML engine, instantiates all models, handles login/logout, settings, favorites. |
| `CalaosConnection.h/cpp` | Network layer. Manages WebSocket (primary) and HTTP (fallback) connections to `calaos_server`. Handles authentication, JSON commands, event polling, camera picture downloads. |
| `HomeModel.h/cpp` | Home data model. Contains rooms (`RoomItem`), light-on tracking (`LightOnModel`). Loaded from server response. |
| `RoomModel.h/cpp` | Room model with IO items (`IOBase`). `IOBase` is the core class representing any home automation device (light, shutter, sensor, scenario, etc.). Also contains `IOCache` (singleton for fast IO lookup), `ScenarioModel`, `ScenarioSortModel`. |
| `Common.h/cpp` | Shared enums (`AppStatus`, `IOType`, `MenuType`, `FavoriteType`, `AudioStatusType`, `EventType`), utility functions, QML registration. |
| `AudioModel.h/cpp` | Audio player model and `AudioImageProvider` for cover art. |
| `CameraModel.h/cpp` | Camera model and `CameraImageProvider` for camera snapshots. |
| `FavoritesModel.h/cpp` | User favorites model (`FavoritesModel`) and home favorites (`HomeFavModel`). |
| `EventLogModel.h/cpp` | Event log model with pagination. |
| `HardwareUtils.h/cpp` | Platform abstraction layer (singleton). Handles network status, keychain, push notifications, alerts, config options. |
| `HardwareUtils_desktop.h/cpp` | Desktop-specific hardware implementation. |
| `CalaosOsAPI.h/cpp` | REST API client for Calaos OS system management (reboot, network config, system info, OS installation, filesystem status). Desktop-only. |
| `ScreenManager.h/cpp` | DPMS screen management (suspend/wakeup). Desktop-only singleton. |
| `CalaosWidgetModel.h/cpp` | Desktop widget system (save/load widget positions). |
| `ControlPanelModel.h/cpp` | Desktop control panel model. |
| `OSInstaller.h/cpp` | Calaos OS installation wizard. Desktop-only. |
| `WeatherInfo.h/cpp` | Weather data model. Desktop-only. |
| `UserInfoModel.h/cpp` | User information model. Desktop-only. |
| `UsbDisk.h/cpp` | USB disk detection for OS installation. Desktop-only. |
| `Machine.h/cpp` | Machine/network info data class. |
| `Lang.h/cpp` | Language model for language selection UI. |
| `RoomFilterModel.h/cpp` | QSortFilterProxyModel for filtering rooms (all, left panel, right panel, scenarios). |
| `NetworkRequest.h/cpp` | Low-level HTTP request helper. |
| `AsyncJobs.h/cpp` | Async job chaining utility. |
| `XUtils.h/cpp` | X11 utility functions. Desktop-only. |
| `qqmlhelpers.h` | QML property macros (`QML_READONLY_PROPERTY`, `QML_WRITABLE_PROPERTY`, `QML_READONLY_PROPERTY_MODEL`, etc.). |

### QML Layer

#### QuickFlux Actions (`qml/quickflux/`)
The app uses the QuickFlux pattern for QML-side actions. Actions are defined in `ActionTypes.qml` as a singleton `KeyTable`. Dispatching is done via `QFAppDispatcher`.

Key actions: `clickHomeboardItem`, `wakeupScreen`, `suspendScreen`, `openCameraSingleView`, `showMainMenu`, `openEventLog`, `openEventPushViewerUuid`, `showNotificationMsg`, `showSensorDetails`, etc.

#### Shared Components (`qml/SharedComponents/`)
Reusable IO widgets mapped to `Common::IOType`:
- `IOLight`, `IOLightDimmer`, `IOLightRGB` - Light controls
- `IOShutter`, `IOShutterSmart` - Shutter/blind controls
- `IOTemp` - Temperature display
- `IOVarBool`, `IOVarInt`, `IOVarString` - Generic variable controls
- `IOScenario` - Scenario trigger
- `IOFavAllLights` - All-lights-off favorite
- `IOSwitch`, `IOPump`, `IOOutlet`, `IOHeater`, `IOBoiler`, `IOAnalogStyled` - Styled variants
- `CalaosSlider`, `CalaosButton`, `CalaosCheck`, `CalaosRadio`, `CalaosCombo` - UI primitives
- `Theme.qml` (singleton) - Theming constants
- `Units.qml` (singleton) - Responsive unit calculations

Module declaration is in `qml/SharedComponents/qmldir`.

#### Desktop UI (`qml/desktop/`)
Full touchscreen kiosk interface: `HomeView`, `RoomDetailView`, `MediaView`, `ConfigView` (network, screen, i18n, user info, updates, installation), `CameraListView`/`CameraSingleView`, `AudioPlayer`, `ScenarioView`, widgets, screensaver, virtual keyboard, dialogs, overlays.

#### Mobile UI (`qml/mobile/`)
Mobile-optimized interface: `LoginView`, `RoomListView`, `RoomDetailView`, `FavoritesListView`/`FavoritesAddView`/`FavoritesEditView`, `CameraListView`/`CameraSingleView`, `MusicListView`, `EventLogView`, `ScenarioView`, `SettingsView`, `ColorPickerView`, pull-to-refresh.

### Platform Code

#### Android (`android/`)
- `HardwareUtils_Android.h/cpp` - JNI bridge for Android-specific features
- `AndroidManifest.xml` - App manifest
- `build.gradle` / `settings.gradle` - Gradle build configuration
- `google-services.json` - Firebase configuration (push notifications)
- Java sources in `android/src/` for native Android integration

#### iOS (`ios/`)
- `HardwareUtils.mm` / `HardwareUtils_iOS.h` - Objective-C++ platform implementation
- `KeychainItemWrapper` - Keychain access for secure credential storage
- `Reachability` - Network reachability monitoring
- `AlertPrompt` - Native alert dialogs
- `PushNotif/` - Push notification handling
- `SimpleKeychain/` - Keychain helper library

## Networking Protocol

The app communicates with `calaos_server` via:

1. **WebSocket** (preferred): Connects to `wss://<host>/api/v3` for real-time bidirectional communication
2. **HTTP fallback**: REST API at `https://<host>/api.php` when WebSocket is not available
3. **API versions**: Supports v2 (HTTP) and v3 (WebSocket) protocols

Events are received for: IO state changes (input/output), room changes, audio player state, scenario changes, camera triggers, push notifications.

## Key QML Context Properties

These are exposed from C++ to QML via `engine.rootContext()->setContextProperty()`:

- `calaosApp` - The `Application` instance
- `homeModel` - `HomeModel`
- `scenarioModel` - `ScenarioSortModel`
- `lightOnModel` - `LightOnModel`
- `audioModel` - `AudioModel`
- `favoritesModel` - `FavoritesModel`
- `favoritesHomeModel` - `HomeFavModel`
- `cameraModel` - `CameraModel`
- `langModel` - `LangModel`
- `eventLogModel` - `EventLogModel`
- Desktop-only: `widgetsModel`, `screenManager`, `userInfoModel`, `controlPanelModel`, `usbDiskModel`, `osInstaller`

## CI/CD Workflows (`.github/workflows/`)

| Workflow | Trigger | Purpose |
|---|---|---|
| `build.yml` | Push to `master` | Dispatches build to `calaos/calaos-build` repo |
| `build_dev.yml` | Manual (version input) | Builds dev package, creates git tag |
| `build_release.yml` | Manual (version input) | Builds release package, creates git tag |
| `build_docker_dev.yml` | Push to `master` | Builds Docker image, pushes to GHCR with `dev` tag, triggers deb build |
| `build_docker_release.yml` | Manual (version increment) | Builds Docker image, pushes to GHCR with `latest` tag, triggers deb build |

## i18n / Translations

- Translation files: `lang/calaos_<lang>.ts` (source) / `.qm` (compiled)
- Currently supported: French (`fr`), German (`de`), English (built-in)
- Auto-translation tool: `lang/auto_translate.go` (Go program using external translation API)
- Language selection at runtime via `Application::setLanguage()` and `HardwareUtils::setConfigOption("lang", code)`

## Desktop Widgets

Desktop-only widget system in `widgets/`:
- `Clock/` - Clock widget
- `Note/` - Sticky note widget
- `Weather/` - Weather widget
- Widget metadata: `calaos_widgets.json`
- Managed by `CalaosWidgetModel` (save/load positions to disk)

## Coding Conventions

- **C++ standard**: C++11 (`CONFIG += c++11`)
- **Property macros**: Use `QML_READONLY_PROPERTY`, `QML_WRITABLE_PROPERTY`, `QML_READONLY_PROPERTY_MODEL` from `qqmlhelpers.h` for QML-exposed properties
- **Model pattern**: All models inherit `QStandardItemModel`. Items inherit both `QObject` and `QStandardItem`.
- **Singleton pattern**: Used for `HardwareUtils`, `IOCache`, `ScreenManager`, `CalaosOsAPI`, `CalaosWidgetModel`, `UserInfoModel`
- **Platform branching**: Use `#ifdef CALAOS_DESKTOP`, `#ifdef CALAOS_MOBILE`, `#ifdef Q_OS_ANDROID`, `#ifdef Q_OS_IOS` for platform-specific code
- **QML module path**: `qrc:/qml/` - imported via `engine.addImportPath("qrc:/qml/")`
- **QML registration**: Custom types registered via `qmlRegisterType` and `qmlRegisterUncreatableType` under the `Calaos` module (version 1.0)
- **Signal/slot**: Mix of old-style `SIGNAL()/SLOT()` macros and modern lambda connections
- **Settings**: `QSettings` with organization `Calaos` and app name `CalaosHome`
- **Credentials**: Stored in platform keychain (iOS Keychain, Android Keystore) via `HardwareUtils`

## Important Notes for AI Agents

1. **Two build targets**: Always consider whether a change applies to mobile, desktop, or both. Check for `#ifdef` guards.
2. **No test suite**: The project currently has no automated tests.
3. **Third-party code**: Do not modify files under `3rd_party/`. These are external libraries.
4. **QML resource files**: When adding new QML or asset files, they must be added to the corresponding `.qrc` resource file (`qml_mobile.qrc`, `qml_desktop.qrc`, `qml_shared.qrc`, `img_mobile.qrc`, `img_desktop.qrc`, `fonts_mobile.qrc`, `fonts_desktop.qrc`).
5. **Source file registration**: When adding new C++ source files, they must be added to either `calaos.pri` (shared), `mobile.pro` (mobile-only), or `desktop.pro` (desktop-only).
6. **IOBase is central**: Most home automation interactions go through `IOBase`. It handles all IO types (lights, shutters, sensors, scenarios, etc.) with a unified interface.
7. **Demo mode**: The app supports a demo mode connecting to `demo.calaos.fr` with credentials `demo`/`demo`.
8. **Version**: Defined in `src/version.h` as `PKG_VERSION_STR`. Set to `"git"` in source, overridden at build time.
