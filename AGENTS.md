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
| Android specifics (build, Firebase, Play Store) | [doc/android.md](doc/android.md) |

## Quick facts

- **Organization**: Calaos (`calaos.fr`)
- **License**: GPLv3+ with App Store / Google Play exception
  (see [COPYING](COPYING) / [COPYING.GPL](COPYING.GPL))
- **Main branch**: `master`
- **Build system**: qmake — [desktop.pro](desktop.pro), [mobile.pro](mobile.pro),
  shared in [calaos.pri](calaos.pri)
- **Variant switch**: `CALAOS_DESKTOP` vs `CALAOS_MOBILE` preprocessor defines
- **Docker image** (desktop): `ghcr.io/calaos/calaos_home`
- **No automated test suite** — validate by building both `.pro` files

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
  [android/android.pri](android/android.pri).
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
