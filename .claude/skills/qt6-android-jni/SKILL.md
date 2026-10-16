---
name: qt6-android-jni
description: Conventions for the Java ↔ C++ bridge of Calaos Home on Android with Qt 6 — QJniObject/QJniEnvironment, Android context, native method registration, threading, exceptions, and Java code that runs without Qt loaded (FirebaseMessagingService). Use when writing or reviewing android/*.cpp, android/src/**/*.java, or Q_OS_ANDROID code in src/.
---

# Qt 6 JNI conventions — Calaos Home

## C++ side

- Use Qt Core's `QJniObject` / `QJniEnvironment` (`<QJniObject>`, `<QJniEnvironment>`). No QtAndroidExtras, no `QAndroidJniObject` (Qt 5).
- Android context: `QNativeInterface::QAndroidApplication::context()`. Do **not** call internal Java classes such as `org.qtproject.qt.android.QtNative` — they change between Qt releases.
- Anything that touches Android UI or must run on the Android main thread: `QNativeInterface::QAndroidApplication::runOnAndroidMainThread(...)`, or do it in Java with `runOnUiThread`.
- **Native method registration**: register with `QJniEnvironment::registerNativeMethods("fr/calaos/calaoshome/<Class>", methods, count)` once `QGuiApplication` exists. It resolves the class through Qt's class loader. Do not define our own `JNI_OnLoad` + `FindClass`: `FindClass` from a native thread uses the wrong class loader.
- Callbacks from Java arrive on the Android thread → hop to the Qt thread with `QMetaObject::invokeMethod(obj, ..., Qt::QueuedConnection)`. Never touch QObjects/QML directly from them.
- After every Java call that can throw: `QJniEnvironment env; if (env.checkAndClearExceptions()) { ... }`.
- `QJniObject::fromString` for strings; check `isValid()` on returned objects before use.
- Never log credentials, push tokens in full, or URLs containing `cn_user`/`cn_pass`.

## Java side

- Package `fr.calaos.calaoshome`, sources in `android/src/fr/calaos/calaoshome/`. Java 17.
- The activity `HardwareUtils extends org.qtproject.qt.android.bindings.QtActivity`. Keep static helpers null-safe: `_context` may be null before `onCreate` or after `onDestroy`.
- Guard newer APIs with `Build.VERSION.SDK_INT` (minSdk is 28). Prefer `ConnectivityManager.getNetworkCapabilities(getActiveNetwork())` over the deprecated `NetworkInfo`.
- **Code that may run without Qt loaded** (`FirebaseMessagingService`, broadcast receivers, workers): the process can be started for a push without the activity, so `libCalaosHome` is not loaded and calling a `native` method throws `UnsatisfiedLinkError`. Persist data (push token, etc.) in `SharedPreferences` and let C++ read it at startup. Only call natives when Qt is known to be running, and catch `UnsatisfiedLinkError`.
- `onMessageReceived` has ~10 s: use an expedited `WorkManager` job for anything longer (image download).
- Runtime permissions (Android 13+ `POST_NOTIFICATIONS`) are requested from the activity, not from a service.
