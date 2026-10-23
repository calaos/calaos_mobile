# Calaos Home

Calaos Home is the official Qt 6 / QML client for the [Calaos](https://calaos.fr)
home automation system. It talks to `calaos_server` (JSON API over WebSocket,
with an HTTP fallback) and is built from a single code base for two kinds of
targets:

| Target | Platform | Build system | Executable |
|---|---|---|---|
| Mobile app | Android (Play Store), iOS (App Store) | CMake | `CalaosHome` |
| Desktop / touchscreen kiosk | Linux (X11), runs on Calaos OS in a container | qmake | `calaos_home` |

The variant is selected at compile time with the `CALAOS_MOBILE` or
`CALAOS_DESKTOP` define. Other repositories of the suite: `calaos_base`
(server), `calaos-container` (system daemon on Calaos OS), `calaos_remote_ui`
(ESP32 panels), `calaos-build` (OS image).

## Features

Common to both variants:

- Rooms and home automation IO: lights (on/off, dimmer, RGB), shutters, switches,
  outlets, sensors and measurements, climate (modes and setpoints taken from the
  server capabilities), scenarios, variables.
- Favorites, cameras, audio players, event log.
- Live updates over WebSocket, automatic reconnection with backoff.
- Languages: English (built in), French, German.

Mobile only (Android and iOS):

- Login screen, credentials in the platform secure storage (iOS Keychain,
  Android Keystore-encrypted storage).
- Push notifications (Firebase Cloud Messaging on Android, APNs plus a
  Notification Service Extension on iOS) that open the event; on Android a
  camera picture is attached when the event has one.
- Safe-area handling, offline banner.

Desktop kiosk only:

- Full-screen UI with an on-screen virtual keyboard and draggable widgets
  (clock, note, weather).
- Embedded web browser and media views when Qt WebEngine is available
  (`HAVE_WEBENGINE`).
- Screen suspend (DPMS), network and language configuration, user info.
- Software update page: checks, upgrades and progress are served by the
  `calaos-container` daemon.
- Reboot, application restart, snapshot rollback and OS installer (the
  installer is only enabled when running from a live medium).

## Requirements

- Qt 6. The CMake build requires 6.6 or later; the build scripts default to
  Qt 6.12.0 (`QT_VERSION`). CI builds the desktop target with the current Arch
  Linux Qt 6.
- Desktop: Qt modules `core widgets network qml quick gui websockets core5compat`,
  optional `webenginequick`; `libx11` and `libxext` (pkg-config). The Arch
  package list used by CI, including the QML runtime modules (virtual keyboard,
  svg, webview, ...), is in `.github/workflows/pr-ci.yml`.
- Android and iOS builds are done on macOS (see below).

## Building

### Desktop kiosk (qmake)

```sh
mkdir build-desktop && cd build-desktop
qmake6 ../desktop.pro PREFIX=/usr
make -j"$(nproc)"
sudo make install          # installs calaos_home in $PREFIX/bin
```

`PREFIX` defaults to `/usr/local`; `$PREFIX/etc` is compiled in as `ETC_DIR`
(one of the configuration search paths, see below). Use `qmake` instead of
`qmake6` if that is how Qt 6 is named on your system.

### Mobile code on a desktop (qmake, for UI work)

`mobile.pro` on Linux or macOS builds the mobile UI as a normal desktop
application, which is handy to iterate on QML:

```sh
mkdir build-mobile && cd build-mobile
qmake6 ../mobile.pro && make -j"$(nproc)"
```

On Android, `mobile.pro` stops with an error; on iOS it is the legacy path
(see `doc/ios.md`).

### Android (CMake, Qt for Android, Gradle)

```sh
./tools/build_android.sh Debug              # arm64-v8a APK
./tools/build_android.sh Release --aab      # four ABIs, Play Store bundle
./tools/build_android.sh Debug --lib-only   # native library only
```

Needs Qt 6.12 Android kits, the Android SDK/NDK, JDK 17 and KDAB
`android_openssl`; locations and the signing variables
(`CALAOS_KEYSTORE`, ...) are documented in the header of the script. minSdk 28,
targetSdk 36. Details: [doc/android.md](doc/android.md).

### iOS (CMake, Xcode)

```sh
./tools/build_ios.sh Debug                # device
./tools/build_ios.sh Debug --simulator    # simulator (Qt >= 6.12)
./tools/build_ios.sh Release --archive    # archive and export
```

Needs macOS, Xcode and the Qt 6.12 iOS kits; minimum iOS 18. Details:
[doc/ios.md](doc/ios.md).

The app version comes from [VERSION.txt](VERSION.txt) (Android `versionName` and
`versionCode` = `MAJOR*10000 + MINOR*100 + PATCH`).

### Tests

`tests/tests.pro` is a standalone qmake project (Qt Test and Qt Quick Test):

```sh
mkdir build-tests && cd build-tests
qmake6 ../tests/tests.pro && make -j"$(nproc)"
QT_QPA_PLATFORM=offscreen make check
```

See [tests/README.md](tests/README.md) (in French). The CI also runs `qmllint`
on `qml/` and `widgets/`, and `tools/check_sources.sh` checks that
`calaos.pri` and `cmake/CalaosHomeSources.cmake` list the same shared sources.

## Running and configuration

### Mobile

Enter the server address, user and password on the login screen. A bare host
is contacted on `wss://<host>/api`, with `https://<host>/api.php` as fallback;
a full `http(s)://` or `ws(s)://` URL is used as is. A demo account
(`demo.calaos.fr`) is built in. TLS certificate errors are currently ignored,
so self-signed servers work.

### Desktop kiosk

```
calaos_home [--config <directory>] [--cache <directory>] [--help]
```

- Settings live in `local_config.xml`, in the first existing directory among
  `~/.config/calaos`, `/etc/calaos`, `$PREFIX/etc/calaos` (else
  `~/.config/calaos` is created). `--config` overrides the directory,
  `--cache` overrides the cache directory (default `~/.cache/calaos`). Both
  must be writable.
- A missing file is created with defaults; the format is:

  ```xml
  <?xml version="1.0" encoding="UTF-8"?>
  <calaos:config xmlns:calaos="http://www.calaos.fr">
    <calaos:option name="calaos_server_host" value="192.168.1.10"/>
  </calaos:config>
  ```

- Options read by the application:

  | Option | Meaning | Default written at creation |
  |---|---|---|
  | `calaos_server_host` | Force the server host; when empty, the server is discovered by a UDP broadcast on port 4545 | none |
  | `cn_user`, `cn_pass` | Credentials used to log in | `user` / `pass` |
  | `show_cursor` | `true` shows the mouse cursor, otherwise it is hidden | `true` |
  | `dpms_enable`, `dpms_standby` | Screen suspend on/off and delay in minutes (set from the UI) | `false` |
  | `latitude`, `longitude` | Location for the weather widget | Paris |
  | `lang` | UI language (`fr`, `de`, or empty for English; set from the UI) | none |
  | `user_emails` | Comma-separated user e-mails (set from the UI) | none |
  | `fw_version` | Written at creation | `0` |

- Without a `calaos_server_host`, the kiosk connects to
  `ws://<host>:5454/api` with `http://<host>:5454/api.php` as fallback
  (port 5454 of `calaos_server`).
- System operations go to the `calaos-container` daemon on
  `http://127.0.0.1:8000`, authenticated with the token in
  `/run/calaos/calaos-ct.token`: reboot, application restart, snapshot
  rollback, OS install, network configuration and software updates.
- The Qt Virtual Keyboard is enabled (`QT_IM_MODULE=qtvirtualkeyboard`). The
  kiosk image starts it from `docker/xinitrc` with
  `QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox`.

## Repository layout

```
src/                C++ backend: models, networking, platform abstraction
qml/desktop/        QML of the kiosk (entry: main.qml)
qml/mobile/         QML of the mobile app (entry: main.qml)
qml/SharedComponents/  IO widgets, climate components, theme (module SharedComponents)
qml/quickflux/      QuickFlux actions and dispatcher
widgets/            Desktop widgets: Clock, Note, Weather
android/            Android platform code (Java/JNI glue, Gradle, manifest, FCM push)
ios/                iOS platform code (Obj-C/Obj-C++, keychain, PushNotif extension)
cmake/              CMake modules for the mobile build (sources, Android, iOS, version)
tools/              build_android.sh, build_ios.sh, check_sources.sh, fake_calaos_api.py
tests/              Qt Test / Qt Quick Test suites
docker/             Dockerfile and X11 launcher of the Calaos OS kiosk image
lang/               Qt Linguist .ts/.qm translations (fr, de)
3rd_party/          Vendored: quickflux, qt-qml-models, drivelist
doc/                Architecture and platform documentation (English)
docs/               Refactoring board and tickets (French)
```

Project files: `desktop.pro` and `mobile.pro` share `calaos.pri`; `CMakeLists.txt`
is the mobile (Android and iOS) build and refuses other platforms. QML and
assets are compiled in through the `.qrc` files at the root.

## CI and release

- `.github/workflows/pr-ci.yml` runs on pull requests and on pushes to
  `master` (it ignores `docs/**` and `*.md`), in an Arch Linux container: build
  of `desktop.pro`, the unit tests, and `qmllint`. No mobile build runs in CI.
- `.github/workflows/publish.yml` is manual (Actions, Publish, Run workflow;
  channel `prerelease`, `patch`, `minor` or `major`). It only runs from `master`
  on a commit whose PR CI is green, bumps the version, builds the kiosk Docker
  image from `docker/` and pushes it to `ghcr.io/calaos/calaos_home` (`:dev` for
  a prerelease, `:latest` for a release, plus the version tag), creates the git
  tag and dispatches the `.deb` build to `calaos/pkgdebs`. A push to `master`
  alone publishes nothing.
- Android and iOS releases are built and uploaded by hand with the scripts above.

## License

GPLv3 or later, with an exception allowing distribution through Apple's App
Store and Google Play: see [COPYING](COPYING) and [COPYING.GPL](COPYING.GPL).
Contributors must agree to it. Privacy policy of the mobile app:
[PRIVACY_POLICY.md](PRIVACY_POLICY.md). Authors: [AUTHORS](AUTHORS).

AI coding agents: see [AGENTS.md](AGENTS.md).
