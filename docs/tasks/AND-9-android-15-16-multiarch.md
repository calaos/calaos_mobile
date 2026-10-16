# AND-9 — Android 15/16 et matrice multi-architecture

| Champ | Valeur |
|---|---|
| Phase | P-DEV — Derniers appareils |
| Taille | M |
| Bloqué par | AND-5, MOB-1 |
| Groupe de conflit | G-ANDROID |
| Variantes | mobile (Android) |

## Contexte

Demande de Raoul le 2026-10-16 : supporter les dernières versions d'Android et vérifier les builds multi-architecture. AND-5 produit l'AAB 4 ABI ; ce ticket vérifie le comportement **à l'exécution** sur les versions récentes et sur chaque architecture exécutable.

Changements de comportement pertinents avec `targetSdk 36` :
- **Geste retour prédictif** activé par défaut sur Android 16 : la navigation retour de l'app repose sur `Keys.onReleased` (`Qt.Key_Back`) dans `qml/mobile/main.qml` — vérifier que le retour dépile toujours le `StackView` et ne ferme pas l'app à tort.
- **Edge-to-edge** imposé (traité par MOB-1, vérifié ici).
- **Grands écrans** (sw ≥ 600 dp) : les restrictions d'orientation/redimensionnement sont ignorées en 36 — vérifier l'affichage tablette/paysage.
- **16 Ko** : déjà vérifié à la compilation (AND-1/2), à confirmer sur l'APK release 4 ABI.

Appareils de test : émulateur `Calaos_API36_arm64` (Android 16, arm64 — les images x86/x86_64 ne tournent pas sur Mac Apple Silicon) ; téléphone Redmi 9 (Android 12, `abilist arm64-v8a,armeabi-v7a`) qui permet de tester la variante 32 bits avec `adb install --abi armeabi-v7a`.

## Fichiers

- `qml/mobile/main.qml` (gestion du retour) si nécessaire
- `android/AndroidManifest.xml` (`android:enableOnBackInvokedCallback`, déclarations grands écrans) si nécessaire
- `cmake/Android.cmake`, `tools/build_android.sh`

## Implémentation

1. Retour prédictif : vérifier le comportement sur API 36 ; corriger (support `OnBackInvokedCallback` via Qt, ou opt-out explicite justifié) pour que le retour navigue dans l'app et ne quitte qu'à la racine.
2. Exécuter l'app sur l'émulateur API 36 : login, navigation, rotation, multi-fenêtre (écran partagé), passage en arrière-plan et retour.
3. Tablette : profil émulateur grand écran (ex. Pixel Tablet, API 36) en paysage.
4. Multi-architecture : à partir du build release 4 ABI d'AND-5, extraire les APK par ABI (`bundletool build-apks`) et installer arm64-v8a (émulateur + téléphone) et armeabi-v7a (téléphone, `--abi armeabi-v7a`) ; x86/x86_64 : vérification statique (présence des libs, alignement 16 Ko, `readelf`), l'exécution étant impossible sur ce Mac — le noter.

## Critères d'acceptation

- Sur émulateur Android 16 : le retour système dépile les vues et ne quitte l'app qu'à la racine ; pas de crash en rotation, multi-fenêtre, arrière-plan/retour.
- Affichage tablette paysage correct (captures).
- L'app tourne en arm64-v8a (émulateur API 36 + téléphone) et en armeabi-v7a (téléphone) — preuve : `adb shell getprop` / `dumpsys package fr.calaos.calaoshome | grep primaryCpuAbi`.
- Libs x86 et x86_64 présentes et alignées 16 Ko dans l'AAB.
- `adb logcat -b crash` vide sur tous les runs.
