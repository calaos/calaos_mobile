# AND-6 — Documentation Android et nettoyage legacy

| Champ | Valeur |
|---|---|
| Phase | P-AND — Android Qt 6 |
| Taille | S |
| Bloqué par | AND-5, AND-7 |
| Groupe de conflit | G-ANDROID + G-DOC |
| Variantes | mobile (Android) |

## Contexte

`doc/android.md` décrit encore le flux qmake + Firebase C++ + chemins Windows. `android/android.pri` et `mobile.pro` contiennent des chemins Windows en dur et le SDK Firebase C++.

## Fichiers

- `doc/android.md`, `doc/mobile.md`, `AGENTS.md`, `README.md` si concerné
- `android/android.pri`, `mobile.pro`
- `docs/BOARD.md` (puce Android de « Hors périmètre »)

## Implémentation

1. Réécrire `doc/android.md` autour du chemin CMake réel : prérequis (matrice de versions), `tools/build_android.sh`, installation sur téléphone, logcat, push FCM Java, notification avec image (AND-7), release AAB + signature.
2. Android dans qmake : retirer le support Android de `mobile.pro` / `android/android.pri` (le chemin CMake est le seul supporté pour Android) ou le réduire à une erreur explicite renvoyant vers `tools/build_android.sh`. Ne pas toucher au chemin qmake iOS ni desktop.
3. Mettre à jour la puce Android de « Hors périmètre » dans `docs/BOARD.md` (points traités par P-AND).
4. Mettre à jour le skill `.claude/skills/qt6-android-build` si des faits ont changé.

## Critères d'acceptation

- `doc/android.md` permet à quelqu'un de neuf de builder et installer l'app en suivant uniquement la doc.
- Plus aucun chemin Windows ni référence au SDK Firebase C++ dans `mobile.pro`, `android/`, `doc/`.
- `qmake desktop.pro` n'est pas affecté.
