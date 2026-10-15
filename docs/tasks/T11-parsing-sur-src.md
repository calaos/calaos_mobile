# T11 — Sweep parsing sûr : toInt/toDouble avec défaut + null-checks dynamic_cast

| Champ | Valeur |
|---|---|
| Phase | P2 — Robustesse réseau |
| Taille | M |
| Bloqué par | T04, T05, T06 (mêmes fichiers) |
| Groupe de conflit | G-MODELS + G-COMMON (helpers) |
| Variantes | desktop + mobile |

## Contexte

Aucun parse numérique de donnée réseau dans `src/` ne vérifie le succès (`grep "toInt(&\|toDouble(&" src/` = 0 hit) : toute valeur malformée devient silencieusement 0 et alimente la logique métier. Sites principaux :
- `src/RoomModel.cpp:415` — `return ioData["state"].toDouble();` (état d'IO)
- `src/AudioModel.cpp:137-148` — volume / elapsed / duration
- `src/EventLogModel.cpp:175` — `data["io_state"].toDouble() > 0`
- `src/HomeModel.cpp:48` — `r["hits"].toString().toInt()`
- `src/RoomModel.cpp:278` — `ioData["hits"].toInt()`

De plus, ~6 `dynamic_cast` restants sont déréférencés sans check (le cas RoomFilterModel a été traité en T04) :
- `src/AudioModel.cpp:29-30, 85-86`
- `src/CameraModel.cpp:27-28, 162-163`
- `src/HomeModel.cpp:171-172`
- `src/RoomModel.cpp:699` (`ScenarioSortModel::lessThan`)

## Fichiers

- `src/Common.h` / `src/Common.cpp` (nouveaux helpers)
- `src/RoomModel.cpp`, `src/AudioModel.cpp`, `src/EventLogModel.cpp`, `src/HomeModel.cpp`, `src/CameraModel.cpp`
- Test des helpers dans `tests/tst_common/`

## Implémentation

1. Ajouter dans `Common` deux helpers statiques (nommage libre mais cohérent) :
   - `static int toIntSafe(const QString &s, int def = 0);` (via `toInt(&ok)`, retourne `def` + `qWarning` si échec)
   - `static double toDoubleSafe(const QString &s, double def = 0.0);`
   - Optionnel : surcharges prenant `QJsonValue` (gère à la fois le cas `isDouble()` natif et le cas string).
2. Remplacer tous les parses réseau listés (et ceux découverts par `grep -n "\.toInt()\|\.toDouble()" src/*.cpp` sur des données venant du serveur) par les helpers. Ne pas toucher aux parses de valeurs internes sûres.
3. Null-checks des `dynamic_cast` listés : pattern `auto *x = dynamic_cast<T*>(...); if (!x) { qWarning() << ...; continue/return; }`.
4. Tests des helpers dans `tst_common` : valeurs valides, chaînes vides, non numériques, valeur par défaut.

Comportement inchangé pour les données valides ; les données invalides donnent désormais une valeur par défaut **explicite** + un warning au lieu d'un 0 silencieux.

## Critères d'acceptation

- Tests T04/T05/T06 toujours verts + tests des helpers verts.
- `grep -n "toDouble()" src/*.cpp` ne montre plus de parse de donnée réseau non gardé (revue manuelle du grep jointe à la PR).
- Plus aucun `dynamic_cast` déréférencé sans check dans les fichiers listés.
- Build desktop + mobile OK.

## Vérification

`make check` ; session complète contre serveur réel/démo (rooms, audio, caméras, event log s'affichent normalement).
