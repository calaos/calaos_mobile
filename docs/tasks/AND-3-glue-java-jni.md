# AND-3 — Glue Java/JNI au standard Qt 6

| Champ | Valeur |
|---|---|
| Phase | P-AND — Android Qt 6 |
| Taille | M |
| Bloqué par | AND-2 |
| Groupe de conflit | G-ANDROID |
| Variantes | mobile (Android) |

## Contexte

Le pont Java ↔ C++ date de Qt 5 et contient des défauts :

- `android/src/fr/calaos/calaoshome/HardwareUtils.java` `getNetworkStatus()` : `getNetworkInfo(TYPE_WIFI)` peut renvoyer `null` (API 29+) → `NullPointerException` ; `NetworkInfo` est déprécié ;
- `_context` statique utilisé sans garde ;
- `android/HardwareUtils_Android.cpp` : `JNI_OnLoad` maison + `FindClass` (mauvais class loader possible) au lieu de `QJniEnvironment::registerNativeMethods` ; exceptions JNI pas toujours nettoyées (`inputTextDialog`) ;
- `src/Application.cpp` (bloc `#ifdef Q_OS_ANDROID` vers la ligne 85) appelle la classe interne `org/qtproject/qt/android/QtNative.activity()`.

Conventions : skill `qt6-android-jni`.

## Fichiers

- `android/src/fr/calaos/calaoshome/HardwareUtils.java`
- `android/HardwareUtils_Android.cpp`, `android/HardwareUtils_Android.h`
- `src/Application.cpp` (bloc Android uniquement)

## Implémentation

1. `getNetworkStatus()` via `ConnectivityManager.getNetworkCapabilities(getActiveNetwork())` (WiFi / cellulaire / aucun), null-safe. Permission `ACCESS_NETWORK_STATE` présente dans le manifest.
2. Gardes null sur `_context` dans les helpers statiques.
3. Enregistrement des natives (`emitDialogTextValid`, `emitDialogCancel`) via `QJniEnvironment::registerNativeMethods` dans `platformInit`, suppression de `JNI_OnLoad`.
4. Nettoyage systématique des exceptions après chaque appel Java.
5. `Application.cpp` : remplacer l'appel à `QtNative` par une API publique (`QNativeInterface::QAndroidApplication::context()`), sans changer le comportement (la densité reste forcée à 1).

**Hors périmètre** : push (AND-4, AND-7).

## Critères d'acceptation

- Sur le téléphone : un dialogue d'alerte natif s'affiche (ex. mauvais identifiants), le dialogue de saisie de texte renvoie la valeur et l'annulation fonctionne.
- `getNetworkStatus()` renvoie la bonne valeur en WiFi, en 4G et en mode avion, sans crash.
- Aucune ligne `JNI DETECTED ERROR`, `UnsatisfiedLinkError` ou exception Java non gérée dans logcat pendant ces tests.
- Plus aucune référence à `QtNative` ni à `JNI_OnLoad` dans le code de l'app.
- `tools/build_ios.sh Debug` passe toujours si `src/` a changé.
