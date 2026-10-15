# T06 — HomeModel : onCache jamais vidé (lumières fantômes après reconnexion)

| Champ | Valeur |
|---|---|
| Phase | P1 — Bugs critiques |
| Taille | S |
| Bloqué par | T02 |
| Groupe de conflit | G-MODELS |
| Variantes | desktop + mobile |

## Contexte

`LightOnModel` maintient un `QHash onCache` (`src/HomeModel.h:60`) pour dédupliquer les lumières allumées. `HomeModel.cpp:29` appelle `lightOnModel->clear()` lors du rechargement — mais `clear()` (hérité de `QStandardItemModel`) ne vide que les **lignes du modèle**, pas le hash `onCache`.

Conséquences après une reconnexion (logout/login) :
1. `addLight` (`HomeModel.cpp:159`) voit `onCache.contains(ioId)` vrai pour des entrées périmées et **refuse d'ajouter des lumières pourtant allumées** → compteur de lumières faux.
2. Les entrées du hash pointent vers des clones `IOBase` libérés (pointeurs pendants).

## Fichiers

- `src/HomeModel.cpp` (autour des lignes 29 et 159)
- `src/HomeModel.h` (si un override de `clear()` est ajouté)
- **Nouveau** : `tests/tst_homemodel/` (+ ajout au `SUBDIRS` de `tests/tests.pro`)

## Implémentation

1. Vider `onCache` au même moment que le clear du modèle. Approche recommandée : ajouter une méthode `LightOnModel::clearAll()` (ou surcharger un slot dédié) qui fait `clear(); onCache.clear();`, et l'appeler depuis `HomeModel.cpp:29`. Vérifier s'il existe d'autres call-sites de `lightOnModel->clear()` (grep) et les migrer aussi.
2. Test : simuler deux cycles load → clear → load avec les mêmes IO lumières allumées ; vérifier que le compteur (`rowCount()` de LightOnModel) est identique après chaque cycle.

## Critères d'acceptation

- Test unitaire vert : après deux cycles load/clear/load, le nombre de lumières allumées est correct (pas 0, pas doublé).
- `make check` global vert ; build desktop + mobile OK.

## Vérification

Manuel : allumer 2 lumières, vérifier le compteur, se déconnecter/reconnecter (ou tuer/relancer l'app) — le compteur affiche à nouveau 2, et « tout éteindre » fonctionne.
