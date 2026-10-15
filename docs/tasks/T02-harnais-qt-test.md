# T02 — Harnais Qt Test minimal

| Champ | Valeur |
|---|---|
| Phase | P0 — Fondations |
| Taille | M |
| Bloqué par | T01 |
| Groupe de conflit | G-BUILD |
| Variantes | infrastructure (aucune) |

## Contexte

Le projet n'a **aucun test automatisé** (vérifié : aucun usage de testlib, aucun `CONFIG += testcase`, aucun répertoire de tests). Tous les tickets suivants du backlog exigent des tests pour sécuriser les refactorings. Ce ticket crée l'infrastructure réutilisable.

## Fichiers (tous nouveaux)

- `tests/tests.pro` — `TEMPLATE = subdirs`, liste des sous-projets de test
- `tests/common.pri` — configuration partagée par tous les tests
- `tests/tst_common/tst_common.pro`
- `tests/tst_common/tst_common.cpp`
- `tests/README.md` — convention d'ajout d'un test

**Ne pas toucher** `desktop.pro` / `mobile.pro` : le projet de tests est un projet qmake autonome.

## Implémentation

1. `tests/common.pri` :
   ```qmake
   QT += testlib core gui network websockets
   CONFIG += c++17 console testcase
   CONFIG -= app_bundle
   INCLUDEPATH += $$PWD/../src
   ```
   Chaque test ajoute à la carte les `.cpp` de `src/` dont il a besoin (ne pas tout linker : certains fichiers tirent des dépendances lourdes comme `HardwareUtils`/`quickflux`).
2. `tests/tst_common/` : premier test réel sur `src/Common.cpp` — round-trip `Common::IOTypeFromString` / `Common::IOTypeToString` (`src/Common.cpp:4-89`) pour **tous** les enums `Common::IOType` de `src/Common.h`, y compris les types stylés `Pump/Outlet/Boiler/Heater` (Common.h:60-63) et le paramètre `style`. Utiliser `QTEST_GUILESS_MAIN` (pas de fenêtre nécessaire).
   - Si `Common.cpp` tire des dépendances non liables en test (ex. quickflux), isoler : compiler uniquement `Common.cpp` + stubs minimes, ou noter la dépendance dans le README pour arbitrage.
3. `tests/README.md` : documenter la convention — un sous-dossier `tst_xxx` par cible, ajout au `SUBDIRS` de `tests.pro`, exécution par `qmake tests/tests.pro && make && make check`.

## Critères d'acceptation

- `cd tests && qmake tests.pro && make && make check` passe, avec au moins le test Common vert.
- Le round-trip couvre 100 % des valeurs de l'enum `IOType` (boucle sur l'enum, pas une liste manuelle partielle).
- `tests/README.md` permet à un agent d'ajouter un nouveau test sans autre contexte.

## Vérification

`make check` en local. Ce harnais sera branché en CI par T03.
