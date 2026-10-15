# T16 — IOTypeRegistry : source de vérité unique du mapping des types d'IO

| Champ | Valeur |
|---|---|
| Phase | P3 — Découplage C++ |
| Taille | L |
| Bloqué par | T11, T15 (mêmes fichiers), T02 |
| Groupe de conflit | G-MODELS + G-COMMON |
| Variantes | desktop + mobile |

## Contexte

Le mapping gui_type/style → IOType/catégorie est répliqué dans **~6 endroits** avec des listes copy-pastées :

1. `src/Common.cpp:4-42` (`IOTypeToString`) et `44-89` (`IOTypeFromString`) — les maps canoniques.
2. `src/RoomModel.cpp:716-808` — `detectOldGuiType`, un mapper legacy de **90 branches** if/else.
3. `src/RoomModel.cpp` — trois listes de gui_type en dur dans `load` (150-154, 191-200, 206-215) décidant visibilité/appendRow.
4. `src/RoomModel.cpp:308-318` (`IOBase::checkFirstState`) et `569-617` (`IOBase::outputChanged`) — branchements light/dimmer/rgb.
5. `src/RoomFilterModel.cpp:80-108` (`resetCache`) + `215-227` (`lessThan`) — classification shutter/light/temp, deux fois.
6. `src/EventLogModel.cpp:149-214` (`EventLogItem::load`) — icône/action par IOType, re-dérivant le set light/pump/outlet/boiler/heater.

Le cas spécial « **Pump/Outlet/Boiler/Heater = lumières stylées** » (`src/Common.h:60-63`, elles doivent alimenter LightOnModel) doit être **mémorisé manuellement dans chacun** de ces endroits ; l'oublier dans un seul casse silencieusement le compteur de lumières. Le concept « is a light » est dupliqué 5 fois.

## Fichiers

- **Nouveaux** : `src/IOTypeRegistry.h`, `src/IOTypeRegistry.cpp` → **à enregistrer dans `calaos.pri`**
- Consommateurs migrés : `src/Common.cpp`, `src/RoomModel.cpp`, `src/RoomFilterModel.cpp`, `src/EventLogModel.cpp`, `src/HomeModel.cpp`
- **Nouveau test** : `tests/tst_iotyperegistry/` (+ `tests/tests.pro`)

## Implémentation

### ⚠️ Écrire le test de non-régression D'ABORD

Avant tout refactoring : énumérer les couples `(gui_type, style)` actuellement gérés par `detectOldGuiType` et les maps de `Common.cpp`, **figer leurs sorties attendues** dans `tests/tst_iotyperegistry/` (table de vérité exhaustive). Ce test doit passer contre l'ancien code, puis contre le registry.

### Le registry

Table statique unique d'entrées :
```cpp
struct IOTypeEntry {
    QString guiType;        // "light", "var_bool", ...
    QString style;          // "" ou "pump"/"outlet"/...
    Common::IOType ioType;
    Category category;      // Light, Shutter, Temp, Sensor, Var, Scenario, Other
    bool isLight;           // alimente LightOnModel — Pump/Outlet/Boiler/Heater = true ICI et nulle part ailleurs
    bool isStyledLight;
    QString oldGuiType;     // mapping legacy de detectOldGuiType
};
```
API : `fromGuiType(guiType, style)`, `category(IOType)`, `isLight(IOType)`, `oldGuiType(...)`, etc.

### Migration des consommateurs

- `Common::IOTypeFromString/ToString` délèguent au registry (garder les signatures publiques, elles sont utilisées partout).
- `RoomModel::detectOldGuiType` → appel au registry (supprimer les 90 branches).
- Les 3 listes de `RoomModel::load` → tests de catégorie du registry.
- `IOBase::checkFirstState` / `outputChanged` → `IOTypeRegistry::isLight()` / catégorie.
- `RoomFilterModel::resetCache`/`lessThan` → `category()`.
- `EventLogItem::load` → `isLight()`/`category()` pour la sélection icône/action.

Migration mécanique, un consommateur à la fois, `make check` entre chaque.

## Critères d'acceptation

- Le test de table de vérité passe **à l'identique** avant et après le branchement du registry (équivalence prouvée).
- `detectOldGuiType` supprimé ou réduit à un appel au registry.
- Grep : plus aucune liste de gui_type en dur dans `RoomModel/RoomFilterModel/EventLogModel/HomeModel`.
- Le concept `isLight` (incluant Pump/Outlet/Boiler/Heater) n'existe plus qu'à UN endroit.
- `make check` global vert ; build desktop + mobile ; session manuelle complète OK (rooms, compteur lumières, event log, filtres).

## Vérification

`make check` + session manuelle sur maison de démo : vérifier notamment que le compteur de lumières compte bien les pompes/prises/chaudières stylées, et que l'event log a les bonnes icônes.
