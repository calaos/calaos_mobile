# AGENTS.md — Calaos Home

Calaos Home is the official client for the [Calaos](https://calaos.fr) home
automation system. It is a Qt 6 / QML codebase that builds two distinct
applications from the same backend, both clients of `calaos_server` (JSON over
WebSocket, HTTP fallback):

- a **desktop / touchscreen kiosk** for Calaos OS (runs in a container; also
  talks to the `calaos-container` daemon on `127.0.0.1:8000` for updates,
  reboot, network),
- a **mobile** app for iOS and Android.

[README.md](README.md) has the user-facing build and configuration reference.

Detailed, LLM-oriented documentation lives under [doc/](doc):

| Variant | Doc |
|---|---|
| Desktop / Calaos OS kiosk | [doc/desktop.md](doc/desktop.md) |
| Mobile (iOS + Android, common architecture) | [doc/mobile.md](doc/mobile.md) |
| iOS specifics (build, signing, push, App Store) | [doc/ios.md](doc/ios.md) |
| Android specifics (CMake build, install, FCM push, Play Store) | [doc/android.md](doc/android.md) |
| CMake migration plan (mobile); Android done, iOS CMake in use | [doc/cmake.md](doc/cmake.md) |

Known stale spots in `doc/`: [doc/desktop.md](doc/desktop.md) section 11 lists
workflows that no longer exist (the only workflows are `pr-ci.yml` and
`publish.yml`), section 12.6 says there is no test suite (there is, see
[Tests](#tests)), and it states C++11 (`calaos.pri` sets C++17). Trust the build
files over these docs.

## Quick facts

- **Organization**: Calaos (`calaos.fr`)
- **License**: GPLv3+ with App Store / Google Play exception
  (see [COPYING](COPYING) / [COPYING.GPL](COPYING.GPL))
- **Main branch**: `master`
- **Build system**: **desktop is qmake** ([desktop.pro](desktop.pro), shared
  [calaos.pri](calaos.pri)); **mobile is CMake** ([CMakeLists.txt](CMakeLists.txt),
  which refuses anything but Android and iOS):
  [tools/build_android.sh](tools/build_android.sh) (see
  [doc/android.md](doc/android.md)) and
  [tools/build_ios.sh](tools/build_ios.sh). [mobile.pro](mobile.pro) is only
  the Linux/macOS desktop test build of the mobile UI (it `error()`s on Android;
  its iOS branch is the legacy qmake path).
- **Variant switch**: `CALAOS_DESKTOP` vs `CALAOS_MOBILE` preprocessor defines
- **Qt**: 6; CMake requires >= 6.6, build scripts default to 6.12.0
  (`QT_VERSION`), CI uses Arch's current Qt 6. C++17 (`calaos.pri`).
- **Version**: [VERSION.txt](VERSION.txt) (semver, read by
  [cmake/Versioning.cmake](cmake/Versioning.cmake)); `PKG_VERSION_STR` in
  [src/version.h](src/version.h) is `"git"` and set by packaging.
- **Agent skills**: [.claude/skills/](.claude/skills) (Android/iOS build, JNI,
  Android review, ticket workflow) hold detailed, Mac-specific procedures.
- **Docker image** (desktop): `ghcr.io/calaos/calaos_home`
- **Tests**: standalone Qt Test / Qt Quick Test project in [tests/](tests), see [Tests](#tests); also validate by building `desktop.pro` (and `mobile.pro` for shared code)

## Repository map (high level)

```
src/                C++ backend (models, networking, platform abstraction)
qml/desktop/        QML for the desktop kiosk (entry: main.qml)
qml/mobile/         QML for the mobile app (entry: main.qml)
qml/SharedComponents/  Reusable IO and climate widgets, theme, units
qml/quickflux/      QuickFlux actions + dispatcher (shared)
widgets/            Desktop draggable widgets (Clock, Note, Weather)
android/            Android platform code (JNI, Gradle, manifest)
ios/                iOS platform code (Obj-C/Obj-C++, keychain, push appex)
cmake/              CMake modules (CalaosHomeSources, Android, iOS, Versioning)
tools/              build_android.sh, build_ios.sh, check_sources.sh, fake_calaos_api.py
tests/              Qt Test / Qt Quick Test suites (standalone qmake project)
docker/             Docker image + xinit launcher for Calaos OS
lang/               Qt Linguist .ts/.qm (fr, de); go.mod only, the auto-translate source is not in the repo
3rd_party/          Vendored dependencies — DO NOT MODIFY
doc/                Architecture/platform docs (English)
docs/               Refactoring board (BOARD.md) and tickets (tasks/), in French
.github/workflows/  CI (pr-ci.yml) + manual publish (publish.yml)
```

## CI and release

- `pr-ci.yml` (PRs and pushes to `master`; ignores `docs/**` and `*.md`; Arch
  Linux container): builds `desktop.pro`, runs the tests, runs `qmllint`
  (only syntax/critical/error diagnostics fail; legacy warnings do not). Android
  and iOS are never built in CI: they are built on a Mac with the scripts.
- Publishing is manual: Actions → Publish → Run workflow, channel
  prerelease|patch|minor|major. A push to `master` does **not** publish. The
  workflow must run on `master`, on a commit that has a green `pr-ci.yml` run
  (`publish.yml` `verify-ci`): a docs-only head commit has no run and is
  refused, publish from a code commit. It builds the **kiosk** Docker image
  (`ghcr.io/calaos/calaos_home`, `:dev` or `:latest` plus the version tag),
  tags the repo and dispatches the `.deb` build to `calaos/pkgdebs`. The mobile
  stores are not touched.

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
- **Calaos OS system calls**: [`CalaosOsAPI`](src/CalaosOsAPI.h) (desktop only):
  HTTP to the `calaos-container` daemon at `http://127.0.0.1:8000`, bearer token
  read from `/run/calaos/calaos-ct.token` (header only, re-read on 401 and on
  reconnect). Uses `/api/v2` (envelope, upgrade jobs) and falls back to the
  legacy unprefixed routes on a plain-text 404 (daemon without `/api/v2`); wire
  rules in [`CalaosOsProtocol`](src/CalaosOsProtocol.h). The update flow is
  [`UpdateManager`](src/UpdateManager.h) (also uses the `/api/v2/events`
  WebSocket, `/api/events` on an old daemon).
- **Desktop settings**: `local_config.xml` (`<calaos:option name= value=>`), looked
  up in `~/.config/calaos`, `/etc/calaos`, `$PREFIX/etc/calaos`, overridable
  with `--config`/`--cache`; keys listed in [README.md](README.md). Mobile uses
  `QSettings` (`config/<key>`) through the base `HardwareUtils`.
- **Networking**: [`CalaosConnection`](src/CalaosConnection.h) — WebSocket
  preferred, HTTP fallback. A bare host becomes `wss://<host>/api` +
  `https://<host>/api.php` on mobile, `ws://<host>:5454/api` +
  `http://<host>:5454/api.php` on desktop
  ([src/CalaosConnection.cpp:371-378](src/CalaosConnection.cpp)); a full
  `http(s)://` or `ws(s)://` URL is used as is. Both the v2 and v3 wire
  formats are decoded by [`CalaosEventDecoder`](src/CalaosEventDecoder.h).
  TLS: trust on first use ([`TlsPinning`](src/TlsPinning.h), CM2). A
  certificate Qt validates is used as is; otherwise the SHA-256 of its
  SubjectPublicKeyInfo must match the pin stored per `host:port` (QSettings
  `tls/pins`), an unknown or changed key fails the attempt (`TrustFailure`, no
  automatic retry) and asks the user. Only the errors of that handshake are
  ignored (`ignoreSslErrors(errors)`, never the blanket form: QNAM caches
  it per channel). Expired or wrong-name certificates pass when the key is
  pinned; revoked or forged ones never do. A pinned host keeps its key even
  behind a CA-valid certificate (checked on `encrypted` and in
  `onWsConnected()`, before any credentials are sent); trusting that
  certificate drops the pin (`caValid`, CM2b): the host is then CA-validated
  only and later renewals with a new key pass silently. The pins are copied to the
  Android push worker (`CredentialStore`) and the iOS extension (App Group).
- **Platform abstraction**: [`HardwareUtils`](src/HardwareUtils.h) +
  per-platform subclasses in
  [src/HardwareUtils_desktop.cpp](src/HardwareUtils_desktop.cpp),
  [ios/HardwareUtils.mm](ios/HardwareUtils.mm),
  [android/HardwareUtils_Android.cpp](android/HardwareUtils_Android.cpp).

For deeper architecture, build instructions, CI specifics, and
platform-by-platform deployment workflow, read the docs under [doc/](doc).

## Build and verify

Build out of tree: the qmake artifacts are not all in `.gitignore`.

```sh
# desktop kiosk (what CI builds)
mkdir -p <build dir> && cd <build dir> && qmake6 <repo>/desktop.pro && make -j"$(nproc)"

# shared source lists must stay in sync (calaos.pri vs cmake/CalaosHomeSources.cmake)
./tools/check_sources.sh

# QML lint, as CI (only syntax/critical/error fail)
"$(qmake6 -query QT_HOST_BINS)/qmllint" -I qml $(find qml widgets -name '*.qml')
```

Android/iOS: `./tools/build_android.sh Debug`, `./tools/build_ios.sh Debug
[--simulator]` (macOS only). `tools/fake_calaos_api.py` is a dev stand-in for
`api.php` to test the Android push picture path.

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
- The desktop binary throws (`std::runtime_error`) at startup if the config or cache directory is not writable ([src/HardwareUtils_desktop.cpp](src/HardwareUtils_desktop.cpp) `initConfigOptions`); pass `--config`/`--cache` for a throwaway run. A missing `local_config.xml` is created with `cn_user=user`, `cn_pass=pass`.
- Desktop command line is parsed by `QCommandLineParser` in `HardwareUtilsDesktop::platformInit`, which declares only `--config`, `--cache` and `--help`. `--force-hdpi` is read by `Application.cpp:114` (non-Android) but is not declared there, so on the kiosk binary it is probably rejected; it is usable with `mobile.pro`'s desktop build.
- Android credentials live in `CredentialStore` (Keystore-encrypted prefs) and must stay readable without Qt running (push picture worker); iOS push extension `PushNotif` shares the keychain/app group with the app. See [doc/android.md](doc/android.md), [doc/ios.md](doc/ios.md).
- The PR CI ignores `*.md` and `docs/**`: a docs-only PR gets no check, and such a commit cannot be published.
- Climate: build the mode chooser from `capabilities.modes` and show the range only with `target_temp_range` (`qml/SharedComponents/ClimateLogic.js`); never hardcode the modes.
