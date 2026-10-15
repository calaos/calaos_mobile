# T04 — RoomFilterModel : resetCache incomplet + dynamic_cast non vérifié

| Champ | Valeur |
|---|---|
| Phase | P1 — Bugs critiques |
| Taille | M |
| Bloqué par | T02 |
| Groupe de conflit | G-MODELS |
| Variantes | desktop + mobile |

## Contexte

`RoomFilterModel::resetCache()` (`src/RoomFilterModel.cpp:58-108`) ne vide que `leftCache` et `rightCache` (lignes 60-61). Les quatre listes membres `shutters`, `lights`, `temps`, `other` (déclarées dans `RoomFilterModel.h`) sont **appendées à chaque appel et jamais vidées** (lignes 82, 86, 90, 92). `resetCache()` est déclenché par `rowsInserted`, `rowsRemoved`, `modelReset` et les changements de filtre.

Conséquences :
1. **Pointeurs pendants** : après un reset du modèle source (reconnexion), les listes contiennent des `IOBase*` déjà détruits ; `lessThan()` (lignes ~215-227) appelle `shutters.contains(lobj)` sur ces pointeurs.
2. **Croissance non bornée** + doublons à chaque reconnexion, faussant le partitionnement gauche/droite.

Bug secondaire : ligne 78, `IOBase *obj = dynamic_cast<IOBase*>(rmodel->getItemModel(i))` est déréférencé ligne 80 **sans null-check** (et `getItemModel` peut retourner `nullptr`, cf. lignes 50-56).

## Fichiers

- `src/RoomFilterModel.cpp` (resetCache lignes 58-108 ; lessThan lignes ~200-227)
- `src/RoomFilterModel.h` (si besoin)
- **Nouveau** : `tests/tst_roomfiltermodel/` (+ ajout au `SUBDIRS` de `tests/tests.pro`)

## Implémentation

1. En tête de `resetCache()`, vider aussi `shutters.clear(); lights.clear(); temps.clear(); other.clear();`.
2. Ligne 78 : vérifier le résultat du `dynamic_cast` — si null, `qWarning()` + `continue`.
3. Vérifier `lessThan()` : les `dynamic_cast` de `lobj`/`robj` (~ligne 208) sont aussi déréférencés sans check — corriger de la même façon (retour d'un ordre stable si null).
4. Test `tests/tst_roomfiltermodel/` : construire un `RoomModel` factice peuplé d'`IOBase` de types variés, appeler `resetCache()` **deux fois**, vérifier que le partitionnement gauche/droite est identique entre les deux appels et que les tailles des listes internes ne doublent pas (exposer un accesseur de test ou vérifier via le comportement du proxy).
   - Note : le constructeur d'`IOBase` (`RoomModel.cpp:263-270`) se connecte aux signaux d'un `CalaosConnection*`. Si une instance est requise, en créer une sans appeler `login()` ; si c'est impossible proprement, tester via le comportement observable du proxy et documenter la limite dans le test.

## Critères d'acceptation

- Test unitaire vert : deux `resetCache()` successifs → résultat stable, pas de doublons.
- Pas de crash quand le modèle source contient un item non-IOBase ou un index invalide.
- `make check` global vert ; build desktop + mobile OK.

## Vérification

`make check`. Manuel (optionnel) : sur desktop, ouvrir une pièce, se déconnecter/reconnecter, rouvrir la pièce — la répartition gauche/droite des IO doit être identique.
