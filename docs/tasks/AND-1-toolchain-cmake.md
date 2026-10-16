# AND-1 — Toolchain Android et configuration CMake

| Champ | Valeur |
|---|---|
| Phase | P-AND — Android Qt 6 |
| Taille | M |
| Bloqué par | — |
| Groupe de conflit | G-ANDROID |
| Variantes | mobile (Android), non-régression iOS |

## Contexte

La version Qt 6 d'Android n'a jamais fonctionné (dernier commit : `84e28a1 fix for android+qt6 (not working yet)`, Qt 6.4). Le build CMake a été écrit pour iOS et jamais exécuté pour Android :

- `CMakeLists.txt:3` déclare `LANGUAGES CXX OBJC OBJCXX` pour toutes les plateformes ;
- `cmake/Android.cmake` inclut `openssl.cmake` / `android_add_openssl_modules`, qui ne correspondent pas au dépôt KDAB `android_openssl` actuel ;
- Firebase C++ SDK obligatoire (`FATAL_ERROR` si `FIREBASE_CPP_SDK_DIR` absent), avec un chemin de libs obsolète (`cxx-stl-c++`) ; le push sera réécrit en Java dans AND-4 ;
- les 4 ABI sont compilées en permanence ;
- aucun script de build (iOS a `tools/build_ios.sh`).

Toolchain installée sur le Mac de dev : NDK 27.2.12479018, JDK 17 (`brew openjdk@17`), KDAB `android_openssl` dans `~/Library/Android/sdk/android_openssl`. Voir le skill `qt6-android-build`.

## Fichiers

- `CMakeLists.txt`, `cmake/Android.cmake`
- `android/HardwareUtils_Android.cpp`, `android/HardwareUtils_Android.h`
- **Nouveau** : `tools/build_android.sh`

## Implémentation

1. `CMakeLists.txt` : langages Objective-C activés uniquement pour iOS (`enable_language(OBJC OBJCXX)` sous `if(IOS)` ou équivalent).
2. `cmake/Android.cmake` :
   - OpenSSL : utiliser le fichier CMake et la fonction réellement fournis par le dépôt KDAB cloné (lire son README / ses `.cmake`), chemin surchargeable par `-DANDROID_OPENSSL_DIR` ;
   - option `CALAOS_ANDROID_PUSH` (défaut `OFF`) : quand elle est OFF, aucune dépendance Firebase. Supprimer le lien vers le SDK Firebase C++ (il disparaît définitivement en AND-4) ;
   - ABI : `QT_ANDROID_ABIS` piloté par une variable de cache (défaut `arm64-v8a`), le build release multi-ABI viendra en AND-5 ;
   - min SDK 28 et target SDK 36 (cf. matrice du skill).
3. `android/HardwareUtils_Android.*` : retirer le code Firebase C++ (FbListener, includes) — le push sera réimplémenté en Java en AND-4. Le reste (dialogues, état réseau) ne change pas ici.
4. `tools/build_android.sh [Debug|Release]` sur le modèle de `tools/build_ios.sh` : exporte `JAVA_HOME`, `ANDROID_SDK_ROOT`, `ANDROID_NDK_ROOT` (surchargeables par l'environnement), configure dans `build-android/` avec Ninja, la toolchain NDK, `CMAKE_PREFIX_PATH` du kit `android_arm64_v8a`, `QT_HOST_PATH` macOS, puis build. Vérifie la présence des chemins avec un message clair.

**Hors périmètre** : Gradle / manifest (AND-2), glue Java (AND-3), push (AND-4). Il est attendu que la génération de l'APK échoue encore à l'étape Gradle.

## Critères d'acceptation

- `tools/build_android.sh Debug` configure sans erreur et produit `libCalaosHome_arm64-v8a.so` (compilation + édition de liens OK).
- Aucune référence restante au SDK Firebase C++ dans `cmake/` et `android/*.cpp|h`.
- `tools/build_ios.sh Debug` passe toujours (non-régression iOS).
- `tools/check_sources.sh` ne signale aucun fichier manquant côté CMake.

## Vérification

Lancer les deux scripts de build, `find build-android -name 'libCalaosHome*.so'`, et noter l'étape à laquelle s'arrête le packaging (attendu : Gradle, traité en AND-2).
