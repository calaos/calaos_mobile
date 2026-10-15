# T22 — Modernisation connect() : SIGNAL/SLOT → pointeurs de membres (sweep src/, sérialisé)

| Champ | Valeur |
|---|---|
| Phase | P3 — Découplage C++ |
| Taille | M |
| Bloqué par | T21 (dernier sweep C++ du backlog — tout G-CONN/G-MODELS/G-APP doit être mergé avant) |
| Groupe de conflit | sweep src-wide — sérialisé |
| Variantes | desktop + mobile |

## Contexte

**59 connexions old-style `SIGNAL()/SLOT()`** subsistent dans src/. Elles sont résolues par comparaison de chaînes **à l'exécution** et échouent **silencieusement** sur une signature mal tapée. Plusieurs portent des signaux critiques, ex. `SIGNAL(sig_light_on(IOBase*))` (`HomeModel.cpp:43,87`) où un mismatch casserait le compteur de lumières sans erreur de compilation.

Répartition : `HomeModel.cpp` (10), `CalaosConnection.cpp` (9), `RoomModel.cpp` (8), `Application.cpp` (5), `RoomFilterModel.cpp` (3), plus `AsyncJobs.cpp`, `WeatherInfo.cpp`, `CameraModel.cpp`, `HardwareUtils_desktop.cpp`.

## Fichiers

- `src/HomeModel.cpp`, `src/CalaosConnection.cpp`, `src/RoomModel.cpp`, `src/Application.cpp`, `src/RoomFilterModel.cpp`, `src/AsyncJobs.cpp`, `src/WeatherInfo.cpp`, `src/CameraModel.cpp`, `src/HardwareUtils_desktop.cpp`

## Implémentation

Remplacement mécanique `connect(a, SIGNAL(x(...)), b, SLOT(y(...)))` → `connect(a, &A::x, b, &B::y)`. Points d'attention :
- **Signaux/slots surchargés** : utiliser `qOverload<...>(&A::x)`.
- Slots privés : la syntaxe pointeur-de-membre y accède depuis la classe elle-même sans problème ; depuis l'extérieur, vérifier la visibilité (passer le slot en public ou connecter via lambda).
- Les `sslErrors` de QNetworkAccessManager ont des signatures const-ref : reprendre la signature exacte.
- La **compilation est le filet de sécurité** : toute erreur de signature devient une erreur de build — c'est le but du ticket.

Aucun autre changement (pas de conversion en lambdas, pas de refactoring opportuniste).

## Critères d'acceptation

- `grep -rn "SIGNAL(" src/` = 0 et `grep -rn "SLOT(" src/` = 0.
- Build desktop + mobile OK ; `make check` vert.
- Session manuelle complète fonctionnelle (login, rooms, compteur lumières, audio, caméras, event log, météo desktop).

## Vérification

`make check` + session manuelle sur les deux variantes — insister sur les fonctionnalités portées par les connexions migrées (compteur de lumières, découverte réseau desktop).
