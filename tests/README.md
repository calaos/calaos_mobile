# Tests unitaires (Qt Test)

Projet qmake **autonome** : il n'est inclus ni par `desktop.pro` ni par `mobile.pro`.
Chaque test lie uniquement les `.cpp` de `src/` dont il a besoin.

## Lancer les tests

```sh
mkdir -p /tmp/calaos_tests_build && cd /tmp/calaos_tests_build
qmake6 <chemin_du_depot>/tests/tests.pro
make -j$(nproc)
make check
```

Toujours construire **hors du dépôt** : les artefacts qmake ne sont pas ignorés par `.gitignore`.

Sur cette machine les outils Qt 6 sont suffixés : `qmake6`, `/usr/lib64/qt6/bin/...`.

Options utiles (passées au binaire de test, pas à `make`) :

```sh
./tst_common/tst_common                 # tous les cas
./tst_common/tst_common -v2             # verbeux
./tst_common/tst_common ioTypeRoundTrip # une seule fonction de test
make check TESTARGS="-v2"               # via make
```

## Ajouter un test

1. Créer `tests/tst_<nom>/` avec deux fichiers.

   `tests/tst_<nom>/tst_<nom>.pro` :

   ```qmake
   TEMPLATE = app
   TARGET = tst_<nom>

   include(../common.pri)

   HEADERS += $$SRC_DIR/<Classe>.h
   SOURCES += $$SRC_DIR/<Classe>.cpp \
              tst_<nom>.cpp
   ```

   `tests/tst_<nom>/tst_<nom>.cpp` :

   ```cpp
   #include <QtTest>
   #include "<Classe>.h"

   class Tst<Nom>: public QObject
   {
       Q_OBJECT
   private slots:
       void monCas_data();   //optionnel : test piloté par les données
       void monCas();
   };

   // ... implémentation ...

   QTEST_APPLESS_MAIN(Tst<Nom>)   //ou QTEST_GUILESS_MAIN si une boucle d'événements
   #include "tst_<nom>.moc"       //est nécessaire (signaux, QTimer, réseau)
   ```

2. Ajouter le sous-répertoire à `tests/tests.pro` :

   ```qmake
   SUBDIRS += \
       tst_common \
       tst_<nom>
   ```

3. Vérifier que `make check` est vert.

## Conventions

- Un répertoire `tst_<nom>/` par classe ou par thème testé ; le `TARGET`, le `.pro`,
  le `.cpp` et l'entrée `SUBDIRS` portent le même nom.
- `common.pri` fournit `QT += testlib`, `CONFIG += c++17 console testcase`,
  `INCLUDEPATH += src/` et la variable **`$$SRC_DIR`** (= `<depot>/src`).
  Un test ne redéfinit jamais ces valeurs, il les complète.
- **Lier le minimum de sources.** N'ajouter un `.cpp` de `src/` que si le lien
  échoue sans lui. Beaucoup de fichiers tirent des dépendances lourdes
  (`HardwareUtils`, `quickflux`, QtQuick, ressources `.qrc`) : si un test ne
  peut pas être lié sans elles, c'est en général le signe qu'il faut d'abord
  découpler la classe testée, pas ajouter des sources.
- Préférer les tests pilotés par les données (`_data()` + `QFETCH`) : un cas
  échoué est identifié par son tag de ligne.
- Quand un test couvre un enum, itérer sur `QMetaEnum` plutôt que sur une liste
  manuelle, pour qu'une valeur ajoutée plus tard fasse échouer le test tant
  qu'elle n'est pas couverte (cf. `enumCoverageIsComplete()` dans `tst_common`).
- Un comportement connu comme incorrect se documente avec `QEXPECT_FAIL` et une
  référence au ticket qui le corrigera ; le jour où il est corrigé, le test
  passe en XPASS et force la mise à jour.
- Nouveaux fichiers en **LF** (une partie du dépôt est en CRLF, ne pas propager).

## Tests existants

| Test | Couvre | Sources de `src/` liées |
|---|---|---|
| `tst_calaoseventdecoder` | `CalaosEventDecoder` : décodage pur des événements v2 (chaînes séparées par des espaces, percent-encodées) et v3 (objets JSON), des cartes `get_state` et des réponses de requête HTTP ; trames réelles des deux API, troncatures, types d'événements inconnus et charges utiles hors format, aucune ne devant faire crasher ni atteindre les modèles (T17) | `CalaosEventDecoder.cpp`, `Common.cpp` (+ `IOTypeRegistry.cpp`) |
| `tst_climatewidget` | Qt Quick Test, en rendu logiciel et en RHI (`QT_QUICK_BACKEND=rhi QSG_RHI_BACKEND=opengl`) : `ClimateLogic.js` (bornes, pas, plage et `min_range`, statut, consigne, boutons −/+), `ClimateDrafts.js` (commandes en vol : un champ n'est suivi que si la valeur envoyée diffère de l'attendue, la réponse de la commande k libère 0..k, une valeur que personne n'a envoyée libère tout, la réponse d'une commande numérotée, acceptée ou refusée, est rattachée à elle par son numéro : un refus ne termine que son vol), `ClimateModel` sur un vrai `IOBase` (commandes, brouillons, refus, décompte du lockout), l'item de liste, la bottom sheet mobile et le dialogue desktop avec leurs sheets / dialogues empilés, et le rendu en pixels du cadran. `CLIMATE_SHOTS_DIR=<dossier>` écrit les captures de chaque état. Les images de l'app (`img_desktop.qrc`) et les polices Roboto sont chargées pour que les captures ressemblent à l'app ; lance son propre moteur QML (offscreen) (MOB-2, MOB-3, MOB-4) | `RoomModel.cpp`, `Common.cpp`, `IOTypeRegistry.cpp` |
| `tst_common` | `Common::IOTypeToString` / `IOTypeFromString` (round-trip sur toutes les valeurs de `Common::IOType`, types stylés inclus), `Common::audioStatus*`, helpers de parsing défensif `Common::toIntSafe` / `toDoubleSafe` / `toLongLongSafe` sur `QString`, `QVariant` et `QJsonValue` — valeur rendue **et** politique de warning (T11) | `Common.cpp` |
| `tst_common` (suite) | `IOTypeRegistry` (T16) : couverture de l'enum, catégorie / `isLight` / `isDimmableLight` / `isMeasurement`, visibilité d'un IO dans une pièce, `styleName`, gui_types média, et table de vérité figée des 88 types du protocole historique (ex-`RoomModel::detectOldGuiType`) | `IOTypeRegistry.cpp` |
| `tst_eventlogmodel` | `EventLogIO::resolve()` : priorité input avant output dans la résolution d'un IO d'événement (T05) | `EventLogModel.h` (gabarit header-only) |
| `tst_homemodel` | `LightOnModel::clear()` vide bien `onCache` : pas de lumières fantômes après reconnexion (T06) | `HomeModel.cpp`, `RoomModel.cpp` (+ doublure locale `FakeConnection`, cf. T18 ; `HardwareUtils*` et `QT += widgets` retirés en T31) |
| `tst_iobase` | `IOBase` seule : objet `device` des items climate (chargement, malformé, fusion de `io_changed`, MOB-1), `sendStringValue` / `sendIntValue` / `sendTrue` / `sendFalse` atteignent la connexion avec le bon sens d'IO, `getItemModel()` sans moteur QML, et garde-fou : `askStateText()` ne doit pas revenir dans le méta-objet (T31) | `RoomModel.cpp`, `Common.cpp`, `IOTypeRegistry.cpp` |
| `tst_httpapiversion` | `CalaosConnection::detectHttpApiV2()` (déclarée *header-only* dans `CalaosConnection.h`) : forme du payload `get_home` qui distingue l'API HTTP v2 de l'ancienne, règle déplacée depuis `RoomModel::load()` ; un payload muet (mode démo, home vide) doit laisser la valeur courante intacte (T18) | aucune (header seul) |
| `tst_pushnotif` | Notifications push partagées iOS/Android (FIX-3) : `PushRegistration` (`register_push` une fois par jeton et par session, renvoi sur rotation, rien hors session), `HardwareUtils` (tap différé hors connexion pris une seule fois, le dernier gagne, tap connecté ouvert tout de suite, `restorePendingNotif()` ; `deviceTokenChanged()` émis seulement sur changement ; `clearDeviceToken()` ; `describeToken()` tronqué), `DeferredNotifGate` seul et piloté comme `Application` (tap pendant chaque reconnexion ouvert, délai d'un login précédent sans effet, tap repris par le login suivant si la session retombe avant l'ouverture), un vrai `CalaosConnection` contre de faux serveurs HTTP (refusant en 400 toute requête sans `cn_user`/`cn_pass`, comme `api.php`), websocket et TLS en process (enregistrement authentifié au login, jeton absent au login puis arrivé, après acceptation du login websocket, sur rotation, pendant un login en vol, après déconnexion, repli websocket → HTTP ; certificat auto-signé généré à la volée par la CLI `openssl`, cas sauté sinon), et aucun jeton ni mot de passe en clair dans les logs, dump debug du repli HTTP compris (construit en debug pour cela). Chaque fonction peut tourner seule | `CalaosConnection.cpp`, `CalaosEventDecoder.cpp`, `HardwareUtils.cpp`, `Common.cpp`, `IOTypeRegistry.cpp` |
| `tst_reconnectpolicy` | `ReconnectPolicy` (déclarée *header-only* dans `CalaosConnection.h`) : séquence de backoff 1/2/4/8/16/30/30 s et son plafond, bornes du jitter ±20 %, garde anti-double-login, inertie du second rapport d'échec, abandon borné sur credentials refusés vs retry infini sur erreur transitoire, tolérance du long-poll (T14) | aucune (header seul) |
| `tst_roomfiltermodel` | `RoomFilterModel::resetCache()` idempotent, partition sans doublon, `lessThan` robuste aux non-`IOBase` (T04) | `RoomFilterModel.cpp`, `RoomModel.cpp` (la doublure de lien `roommodel_stub.cpp` a disparu en T31) |
