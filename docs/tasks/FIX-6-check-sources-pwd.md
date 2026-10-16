# FIX-6 — tools/check_sources.sh : faux positifs sur les chemins `$$PWD/src/...`

| Champ | Valeur |
|---|---|
| Phase | P-FIX — Petits bugs constatés pendant P-AND |
| Taille | S |
| Bloqué par | — |
| Groupe de conflit | G-BUILD |
| Variantes | outillage |

## Contexte

`tools/check_sources.sh` compare `calaos.pri` et `cmake/CalaosHomeSources.cmake`. Son extraction côté qmake (`grep -E '^\s+src/'`) ignore les entrées écrites `$$PWD/src/...` et les fichiers sur la ligne `SOURCES += src/main.cpp` : le script sort en erreur alors que les listes sont cohérentes (constaté pendant AND-1, AND-2, AND-4). Un contrôle toujours rouge ne protège plus de rien.

## Fichiers

- `tools/check_sources.sh`

## Implémentation

Normaliser les chemins qmake (`$$PWD/` retiré, entrées sur la même ligne que `SOURCES +=`/`HEADERS +=` prises en compte, continuations `\`), et ne comparer que les fichiers partagés (ceux propres à desktop ou à une plateforme ne doivent pas apparaître). Garder le format de sortie actuel.

## Critères d'acceptation

- Sur l'état actuel du repo : le script sort en 0.
- Retirer volontairement un fichier de `cmake/CalaosHomeSources.cmake` (sans commit) → le script sort en 1 et nomme ce fichier ; idem côté `calaos.pri`.
- Compatible bash 3.2 (macOS) et Linux (CI).
