# AGENTS.md — Calaos Home

Calaos Home is the official client for the [Calaos](https://calaos.fr) home
automation system. It is a Qt 6 / QML codebase that builds two distinct
applications from the same backend:

- a **desktop / touchscreen kiosk** for Calaos OS,
- a **mobile** app for iOS and Android.

Detailed, LLM-oriented documentation lives under [doc/](doc):

| Variant | Doc |
|---|---|
| Desktop / Calaos OS kiosk | [doc/desktop.md](doc/desktop.md) |
| Mobile (iOS + Android, common architecture) | [doc/mobile.md](doc/mobile.md) |
| iOS specifics (build, signing, push, App Store) | [doc/ios.md](doc/ios.md) |
| Android specifics (CMake build, install, FCM push, Play Store) | [doc/android.md](doc/android.md) |
| **CMake migration plan (mobile)** — in progress | [doc/cmake.md](doc/cmake.md) |

## Quick facts

- **Organization**: Calaos (`calaos.fr`)
- **License**: GPLv3+ with App Store / Google Play exception
  (see [COPYING](COPYING) / [COPYING.GPL](COPYING.GPL))
- **Main branch**: `master`
- **Build system**: qmake for desktop and iOS — [desktop.pro](desktop.pro),
  [mobile.pro](mobile.pro), shared in [calaos.pri](calaos.pri); **Android is
  CMake only** ([CMakeLists.txt](CMakeLists.txt),
  [tools/build_android.sh](tools/build_android.sh), see
  [doc/android.md](doc/android.md)); iOS also builds with CMake
  ([tools/build_ios.sh](tools/build_ios.sh))
- **Variant switch**: `CALAOS_DESKTOP` vs `CALAOS_MOBILE` preprocessor defines
- **Docker image** (desktop): `ghcr.io/calaos/calaos_home`
- **Tests**: standalone Qt Test / Qt Quick Test project in [tests/](tests), see [Tests](#tests); also validate by building both `.pro` files

## Repository map (high level)

```
src/                C++ backend (models, networking, platform abstraction)
qml/desktop/        QML for the desktop kiosk (entry: main.qml)
qml/mobile/         QML for the mobile app (entry: main.qml)
qml/SharedComponents/  Reusable IO widgets, theme, units
qml/quickflux/      QuickFlux actions + dispatcher (shared)
widgets/            Desktop draggable widgets (Clock, Note, Weather)
android/            Android platform code (JNI, Gradle, manifest)
ios/                iOS platform code (Obj-C/Obj-C++, keychain, push appex)
docker/             Docker image + xinit launcher for Calaos OS
lang/               Qt Linguist .ts/.qm + Go auto-translate tool
3rd_party/          Vendored dependencies — DO NOT MODIFY
.github/workflows/  CI/CD (desktop pkg + Docker)
```

## Where to find what

- **Add a new C++ source**: shared → [calaos.pri](calaos.pri); desktop-only →
  [desktop.pro](desktop.pro); iOS-only → [ios/ios.pri](ios/ios.pri); Android-only →
  [cmake/Android.cmake](cmake/Android.cmake) (CMake only; also keep shared
  sources in sync with [cmake/CalaosHomeSources.cmake](cmake/CalaosHomeSources.cmake),
  checked by `tools/check_sources.sh`).
- **Add a new QML/asset file**: register in the matching `.qrc`
  ([qml_desktop.qrc](qml_desktop.qrc), [qml_mobile.qrc](qml_mobile.qrc),
  [qml_shared.qrc](qml_shared.qrc), [img_desktop.qrc](img_desktop.qrc),
  [img_mobile.qrc](img_mobile.qrc), [fonts_desktop.qrc](fonts_desktop.qrc),
  [fonts_mobile.qrc](fonts_mobile.qrc), [widgets.qrc](widgets.qrc)).
- **Add a new shared QML component**: also declare it in
  [qml/SharedComponents/qmldir](qml/SharedComponents/qmldir).
- **Home automation IO**: extend [`IOBase`](src/RoomModel.h) and the matching
  widget under [qml/SharedComponents/](qml/SharedComponents).
- **Calaos OS system calls**: [`CalaosOsAPI`](src/CalaosOsAPI.h) (desktop only).
- **Networking**: [`CalaosConnection`](src/CalaosConnection.h) — WebSocket
  (`wss://<host>/api/v3`) preferred, HTTP fallback (`https://<host>/api.php`).
- **Platform abstraction**: [`HardwareUtils`](src/HardwareUtils.h) +
  per-platform subclasses in
  [src/HardwareUtils_desktop.cpp](src/HardwareUtils_desktop.cpp),
  [ios/HardwareUtils.mm](ios/HardwareUtils.mm),
  [android/HardwareUtils_Android.cpp](android/HardwareUtils_Android.cpp).

For deeper architecture, build instructions, CI specifics, and
platform-by-platform deployment workflow, read the docs under [doc/](doc).

## Tests

`tests/tests.pro` is a standalone qmake project (not included by `desktop.pro` / `mobile.pro`): Qt Test suites in `tests/tst_*`, plus QML cases under `tst_climatewidget` (Qt Quick Test). Build out of tree and run `make check`; CI does the same with `QT_QPA_PLATFORM=offscreen` (`.github/workflows/pr-ci.yml:100-112`). Details in [tests/README.md](tests/README.md).

```sh
mkdir -p <build dir outside the repo> && cd <build dir>
qmake6 <repo>/tests/tests.pro && make -j"$(nproc)" && QT_QPA_PLATFORM=offscreen make check
```

## Pitfalls

Cross-repo notes (wire format, capabilities, rules): [calaos_base/docs/18_stack_notes.md](https://github.com/calaos/calaos_base/blob/master/docs/18_stack_notes.md) (in French).

- **Overlays are not children of the page.** `qml/mobile/BottomSheet.qml:22` sets `parent: Overlay.overlay` and `qml/desktop/PopupBase.qml:35` reparents the popup to the overlay layer. A `StackView` pop therefore does not close them: close them explicitly.
- `qml/desktop/OverlayLayer.qml:15` keeps the stack of open popups (the last one is on top). `closeOnDisconnect` (`PopupBase.qml:20`) is honoured by `OverlayLayer.qml:43-45` when the app leaves the logged-in state; it is off by default so a reconnect does not discard what the user is typing.
- Climate: build the mode chooser from `capabilities.modes` and show the range only with `target_temp_range` (`qml/SharedComponents/ClimateLogic.js`); never hardcode the modes.
