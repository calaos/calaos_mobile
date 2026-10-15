# T01 — Hygiène build : C++17 et globs lupdate

| Champ | Valeur |
|---|---|
| Phase | P0 — Fondations |
| Taille | S |
| Bloqué par | — |
| Groupe de conflit | G-BUILD |
| Variantes | desktop + mobile |

## Contexte

Le projet est en Qt 6 mais déclare `CONFIG += c++11` (`calaos.pri:10`). Qt 6 exige C++17 au minimum ; le build ne fonctionne aujourd'hui que parce que les mkspecs Qt 6 forcent silencieusement `-std=c++17`. Le code utilise d'ailleurs déjà des features post-C++11 (`qsizetype` dans `WeatherInfo.cpp`, `std::function` dans `CalaosOsAPI.cpp`). La déclaration est trompeuse et doit être alignée.

Par ailleurs, le bloc `lupdate_only` de `desktop.pro:78-90` référence `src/android/*.cpp`, `src/ios/*.cpp`, `src/ios/*.m`, `src/ios/*.mm`, `src/android/*.h`, `src/ios/*.h` — **ces répertoires n'existent pas**. Le code plateforme vit à la racine : `android/` et `ios/`. Conséquence : les chaînes traduisibles de la couche plateforme ne sont jamais extraites par `lupdate`.

## Fichiers

- `calaos.pri` (ligne 10)
- `desktop.pro` (bloc `lupdate_only`, lignes 78-90)

## Implémentation

1. `calaos.pri:10` : `CONFIG += c++11` → `CONFIG += c++17`.
2. `desktop.pro` : dans le bloc `lupdate_only`, remplacer les chemins fantômes par les vrais emplacements du code plateforme :
   - `src/android/*.cpp` → `android/*.cpp` (contient `HardwareUtils_Android.cpp`)
   - `src/ios/*.cpp`, `src/ios/*.m`, `src/ios/*.mm` → `ios/*.m`, `ios/*.mm`
   - `src/android/*.h` → `android/*.h` ; `src/ios/*.h` → `ios/*.h`
   - Vérifier avec `ls android/*.cpp ios/*.mm` que les globs matchent réellement des fichiers avant de valider.

Aucune modification de code C++/QML.

## Critères d'acceptation

- `qmake desktop.pro && make` compile sans erreur.
- `qmake mobile.pro` (génération du Makefile) passe sans erreur.
- `lupdate desktop.pro -ts /tmp/test_lupdate.ts` ne produit plus d'avertissement « directory not found » sur src/android ou src/ios.

## Vérification

Build desktop complet en local. Pas de test automatisé requis (le harnais arrive en T02).
