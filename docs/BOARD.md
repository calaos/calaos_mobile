# BOARD — Backlog de refactoring Calaos Home

Suivi du backlog de refactoring (robustesse, maintenabilité, bonnes pratiques). Chaque ticket est spécifié dans [`docs/tasks/`](tasks/) et est auto-porteur : il peut être confié tel quel à un agent de développement autonome.

## Règles d'utilisation

- **Le statut vit ici et uniquement ici** (les fichiers de ticket sont des specs immuables). Statuts : `À faire` → `En cours` → `En revue` → `Terminé`.
- Un agent qui prend un ticket passe son statut à `En cours` (une ligne modifiée dans ce fichier, dans le même commit/branche que son travail).
- Un ticket ne peut être pris que si **tous ses tickets « Bloqué par » sont `Terminé`**.
- Deux tickets ne peuvent être `En cours` simultanément que s'ils appartiennent à des **groupes de conflit différents** (voir colonne Groupe). Exception tolérée : les appends sur `calaos.pri`, `tests/tests.pro` et les `.qrc` (conflits triviaux à résoudre au merge).
- `Terminé` = critères d'acceptation du ticket validés + `make check` vert + CI verte.
- Rappels systématiques pour tout ticket : nouveau fichier C++ → enregistrer dans `calaos.pri` (partagé) ou `desktop.pro`/`mobile.pro` ; nouveau QML → enregistrer dans le bon `.qrc` ; toujours considérer les **deux variantes** de build (`CALAOS_DESKTOP` / `CALAOS_MOBILE`).

## P0 — Fondations (séquentiel, barrière : CI verte avant P1)

| ID | Titre | Taille | Bloqué par | Groupe | Statut |
|---|---|---|---|---|---|
| [T01](tasks/T01-build-cpp17.md) | Hygiène build : C++17 et globs lupdate | S | — | G-BUILD | Terminé |
| [T02](tasks/T02-harnais-qt-test.md) | Harnais Qt Test minimal | M | T01 | G-BUILD | Terminé |
| [T03](tasks/T03-ci-pr.md) | CI de PR : build + tests + qmllint | M | T01, T02 | G-BUILD | Terminé |

## P1 — Bugs critiques

| ID | Titre | Taille | Bloqué par | Groupe | Statut |
|---|---|---|---|---|---|
| [T04](tasks/T04-roomfiltermodel-resetcache.md) | RoomFilterModel : resetCache incomplet + cast non vérifié | M | T02 | G-MODELS | Terminé |
| [T05](tasks/T05-eventlogmodel-output-io.md) | EventLogModel : résolution des IO output cassée | S | T02 | G-MODELS | Terminé |
| [T06](tasks/T06-homemodel-oncache.md) | HomeModel : onCache jamais vidé (lumières fantômes) | S | T02 | G-MODELS | Terminé |
| [T07](tasks/T07-bugs-qml-enums.md) | Bundle bugs QML : enums, iconState, densité², action flux | S | T03 | G-QML-SHARED | Terminé |
| [T08](tasks/T08-calaosconnection-fixes.md) | CalaosConnection : correctifs ciblés pré-refactoring | M | T02 | G-CONN | Terminé |
| [T09](tasks/T09-weatherinfo-fuite.md) | WeatherInfo : fuite mémoire forecastDataClear | S | — | indépendant | Terminé |
| [T10](tasks/T10-gardes-nan-qml.md) | Bundle gardes NaN / division par zéro QML | S | T03 | QML (disjoint T07) | Terminé |

Note : T04/T05/T06 sont parallélisables entre eux (fichiers disjoints, seule collision = append de `tests/tests.pro`).

## P2 — Robustesse réseau & sécurité

| ID | Titre | Taille | Bloqué par | Groupe | Statut |
|---|---|---|---|---|---|
| [T11](tasks/T11-parsing-sur-src.md) | Sweep parsing sûr : toInt/toDouble + null-checks | M | T04, T05, T06 | G-MODELS + G-COMMON | En revue |
| [T13](tasks/T13-timeouts-http.md) | Timeouts HTTP + durcissement long-poll | S | T08 | G-CONN | En revue |
| [T14](tasks/T14-reconnexion-backoff.md) | ⚠️ Machine à états de reconnexion + backoff | L | T13 | G-CONN + G-APP | En revue |
| [T15](tasks/T15-camera-polling-imageproviders.md) | Polling caméra maîtrisé + image providers thread-safe | M | T11 | G-MODELS | En revue |

## P3 — Découplage C++

| ID | Titre | Taille | Bloqué par | Groupe | Statut |
|---|---|---|---|---|---|
| [T16](tasks/T16-iotyperegistry.md) | IOTypeRegistry : source de vérité unique des types d'IO | L | T11, T15, T02 | G-MODELS + G-COMMON | À faire |
| [T17](tasks/T17-calaoseventdecoder.md) | Extraction du décodeur d'événements | L | T14 | G-CONN | À faire |
| [T18](tasks/T18-decouplage-roommodel-connection.md) | Découpler RoomModel → CalaosConnection | S | T16, T17 | G-MODELS + G-CONN | À faire |
| [T19](tasks/T19-favoritesmodel-favtypes.md) | FavoritesModel : types non-FavIO (TODO!) | M | — | indépendant | À faire |
| [T20](tasks/T20-qqmlhelpers-garde-egalite.md) | qqmlhelpers : garde d'égalité dans les setters | S | T16 | G-COMMON | À faire |
| [T21](tasks/T21-jsonkeys.md) | Constantes de clés JSON (sweep src/) | M | T16, T17, T18 | sweep src — sérialisé | À faire |
| [T22](tasks/T22-connect-modernes.md) | connect() modernes : SIGNAL/SLOT → pointeurs (sweep src/) | M | T21 | sweep src — sérialisé | À faire |

## P4 — Consolidation QML

| ID | Titre | Taille | Bloqué par | Groupe | Statut |
|---|---|---|---|---|---|
| [T23](tasks/T23-iobinarydevice.md) | Widget IO binaire générique (5 widgets fusionnés) | M | T07 | G-QML-SHARED | À faire |
| [T24](tasks/T24-delegate-map-qmldir.md) | ItemListView : delegate map + qmldir complet | M | T23 | G-QML-SHARED | À faire |
| [T25](tasks/T25-pagescaffold-desktop.md) | PageScaffold desktop : squelette header/footer | M | T07 | G-QML-DESKTOP | À faire |
| [T26](tasks/T26-mediawebview-onglets.md) | MediaWebView : source de vérité unique des onglets | M | T03 | G-QML-DESKTOP | À faire |
| [T27](tasks/T27-sweep-units-dp.md) | Unification sizing : Units.dp partout (sweep) | L | T10, T23-T26 | G-QML-SWEEP — sérialisé | À faire |
| [T28](tasks/T28-sweep-theme-couleurs.md) | Sweep couleurs : tokens Theme partout | L | T27 | G-QML-SWEEP — sérialisé | À faire |

## P5 — Polish

| ID | Titre | Taille | Bloqué par | Groupe | Statut |
|---|---|---|---|---|---|
| [T29](tasks/T29-imports-qml-modernes.md) | Modernisation imports QML (Qt5Compat → MultiEffect) | M | T28 | G-QML-SWEEP | À faire |
| [T30](tasks/T30-nettoyage-final.md) | Nettoyage final : logs, placeholders, formatters | S | T28, T29 | G-QML-SWEEP | À faire |

## Diagramme de parallélisation

Chaque colonne est une lane exécutable par un agent indépendant ; `→` = séquence obligatoire, `‖` = parallèle.

```
P0   T01 → T02 → T03                          (lane unique G-BUILD)

P1   G-MODELS          G-CONN    G-QML        MISC
     T04 ‖ T05 ‖ T06    T08       T07 ‖ T10    T09

P2   G-MODELS          G-CONN(+APP)
     T11 → T15          T13 → T14              (2 lanes en parallèle)

P3   G-MODELS/COMMON      G-CONN     MISC
     T16 ────────┐        T17 ──┐    T19 ‖ T20
                 └── T18 ←──────┘
     puis sweeps sérialisés : T21 → T22

P4   G-QML-SHARED      G-QML-DESKTOP
     T23 → T24          T25 ‖ T26
     puis sweeps sérialisés : T27 → T28

P5   T29 → T30
```

**Barrières de synchronisation** : fin de P0 (CI verte requise) · T14 avant T17 · T16+T17+T18 avant T21 · tous les tickets QML ciblés avant T27.

## Tickets à risque — vérification manuelle obligatoire (desktop ET mobile)

| Ticket | Risque | Vérification |
|---|---|---|
| **T14 Backoff** | Délais de reconnexion modifiés ; suppression du logout sur erreur transitoire | Coupure serveur 2 min → séquence 1/2/4/8…30 s sans chevauchement, un seul login à la reprise ; mode avion mobile ; suspend/resume desktop ; mauvais mot de passe → pas de retry infini. |
| **T13 Timeouts** | Un long-poll légitime pourrait être tué si mal calibré | 30 min sur serveur calme : aucun cycle logout/login parasite. |
| **T15 Caméras** | Cadence de rafraîchissement modifiée | Multi-caméras 10 min + navigation rapide : débit stable, pas de gel. |
| **T20 qqmlhelpers** | Une vue dépendant des notifications redondantes ne se rafraîchirait plus | Parcours complet : rooms, favoris, **audio (position de lecture)**, caméras, event log. |
| **T23/T27/T28 (visuels)** | Changements de pixels potentiels partout | Campagne de screenshots avant/après sur les vues principales des deux variantes. |

## Hors périmètre — backlog futur

Explicitement exclus de ce backlog, à traiter dans une phase dédiée :

- **TLS TOFU / pinning d'empreinte certificat** (ex-T12, spec conservée dans `docs/tasks/T12-tls-tofu.md`) — écarté par Raoul le 2026-08-10 : verrouiller le certificat est trop risqué pour le parc installé, un renouvellement côté serveur couperait les clients. `ignoreSslErrors` reste donc en place. T13 et T14 n'en dépendaient pas sur le fond : ils n'étaient sérialisés derrière lui que parce que les trois touchent `CalaosConnection.cpp` (groupe G-CONN). T13 est repositionné derrière T08.
- **Migration CMake** (qmake est déprécié pour Qt6) — préalable recommandé au split transport complet de CalaosConnection.
- **Android** : `targetSdkVersion 29` (bloque les mises à jour Play Store, minimum requis 34), **OpenSSL 1.0.2r embarqué (EOL 2019, critique)**, toolchain Gradle 2021 + `jcenter()` mort, credentials en clair dans QSettings (chantier Keystore).
- **iOS** : APIs UIKit dépréciées (`UIAlertView`…), `KeychainItemWrapper` mort à supprimer, étapes Xcode manuelles non scriptables.
- **Docker** : image de base non pinnée, fetch AUR sans checksum.
- **Version applicative** : `src/version.h` codé en dur à `"git"` — injecter la vraie version au build.
- **i18n** : catalogues .ts périmés, `auto_translate.go` cassé par défaut, langues it/es esquissées mais absentes.
- Split transport WS/HTTP complet de CalaosConnection (T17 capture l'essentiel de la valeur testabilité).
