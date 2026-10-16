# QT-1 — Migration Qt 6.11.0 → Qt 6.12.0 (iOS + Android)

| Champ | Valeur |
|---|---|
| Phase | P-DEV — Derniers appareils |
| Taille | M |
| Bloqué par | AND-4 |
| Groupe de conflit | G-ANDROID + G-IOS + G-BUILD |
| Variantes | mobile (iOS, Android) ; desktop hors périmètre |

## Contexte

Demande de Raoul le 2026-10-16 : passer à la version de Qt la plus récente. Au 2026-10-16 : **Qt 6.12.0** (build 202609280345) ; la 6.11 est en 6.11.2. Les builds mobiles CMake utilisent aujourd'hui Qt 6.11.0 (`~/Qt/6.11.0`, chemins en dur dans `tools/build_ios.sh` et valeurs par défaut de `tools/build_android.sh`).

Qt 6.12.0 est installé dans `~/Qt/6.12.0` (macOS, iOS device + simulateur arm64, Android 4 ABI, addons qt5compat, imageformats, shadertools, websockets, webchannel, webview, virtualkeyboard, serialport). Nouveauté utile : tranches simulateur iOS arm64 (exploitées par IOS-1).

## Fichiers

- `tools/build_ios.sh`, `tools/build_android.sh` (version de Qt en un seul endroit, surchargeable)
- `CMakeLists.txt` (`find_package(Qt6 …)`, `qt_standard_project_setup(REQUIRES …)`, politiques Qt)
- `cmake/iOS.cmake`, `cmake/Android.cmake`
- `android/build.gradle`, `android/AndroidManifest.xml` (reportés depuis les modèles Qt 6.12)
- Code C++/QML seulement si une API supprimée/modifiée l'impose
- Skills `.claude/skills/qt6-android-build`, `qt6-ios-build` (matrice de versions)

## Implémentation

1. Lire le changelog Qt 6.12.0 (et ceux de 6.11.1/6.11.2) pour Core, Gui, Qml, Quick, Network, WebSockets, Svg, la plateforme Android et iOS ; lister les changements qui touchent l'app.
2. Version de Qt définie à un seul endroit par script (variable surchargeable, défaut 6.12.0).
3. Android : comparer les modèles `~/Qt/6.12.0/android_*/src/android/templates/` avec ceux de 6.11 et reporter les changements dans `android/build.gradle` et `android/AndroidManifest.xml` en conservant nos ajouts (Firebase, service FCM, règles de sauvegarde, permissions) ; aligner NDK, AGP/Gradle, JDK, min/target SDK sur ce que demande Qt 6.12 (installer le NDK requis via `sdkmanager` si différent de 27.2).
4. iOS : vérifier la cible de déploiement minimale imposée par Qt 6.12 et l'ajuster si nécessaire.
5. Corriger les avertissements de compilation/dépréciations nouveaux introduits par 6.12 dans le code de l'app (pas dans `3rd_party/`, sauf blocage — à signaler).
6. Ne pas supprimer `~/Qt/6.11.0` (retour arrière possible).

**Hors périmètre** : build desktop (qmake/Docker Arch), nouvelles fonctionnalités Qt 6.12.

## Critères d'acceptation

- `tools/build_ios.sh Debug` et `tools/build_android.sh Debug` passent avec Qt 6.12.0 (logs montrant les chemins 6.12.0).
- iPhone (devicectl) et téléphone Android (adb) : l'app s'installe, démarre, se connecte au serveur (wss), navigation de base sans crash ; jeton push toujours obtenu (log tronqué) sur les deux plateformes.
- Aucun nouvel avertissement QML (`qmllint` iso-baseline sur `qml/mobile`) ni nouvel avertissement C++ dans le code de l'app.
- Alignement 16 Ko toujours vérifié (Android) ; matrice de versions des skills à jour.
- Liste des changements Qt 6.12 pertinents jointe au rapport.
