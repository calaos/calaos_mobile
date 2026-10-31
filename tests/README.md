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
| `tst_adminrelay` | Mises à jour de la box depuis le mobile (CM1). `AdminRelay` contre un faux transport : `admin_unlock` (session gardée en mémoire, `expires_in` plafonné à 900 s, expiration absolue), `bad_password`, corrélation par `msg_id` (réponses dans le désordre, `msg_id` inconnu, réponse tardive après le délai ignorée), délais 5 s / 125 s pour `update_check`, portes `https_required`, `admin_not_set`, `unavailable`, `too_old`, `unreachable`, `old_server` (silence sans aucune réponse admin sur ce socket ; pas pendant un upgrade), `bad_session` silencieux, session perdue avec le socket, à l'acceptation d'une nouvelle clé TLS et sur `admin_event admin_set/admin_reset`, aucun `admin_unlock` pendant une question de certificat, sur un socket non `wss` (porte `https_required`), sur le repli HTTP (porte `websocket_required`) ni avant une réponse admin sur le socket courant, porte remise à `idle` à chaque perte du socket, session `admin_session` refusée si elle n'est pas 64 hexadécimaux, requêtes faites avec la session échouées (`session_lost`) à sa fin, délai dépassé verrouillé → `unreachable`, rien envoyé hors WebSocket, ni le mot de passe admin ni la session dans les logs ; vrai `CalaosConnection` contre un faux calaos_server WebSocket en 127.0.0.1, en `wss` avec un certificat auto-signé épinglé (fabriqué par la CLI openssl) et en `ws` (vue foyer sans session puis vue complète, aucun mot de passe sur `ws://`, mot de passe seulement dans `admin_unlock`, session perdue à la chute du serveur, serveur ancien muet, `https_required`) ; `AdminRelayBackend` + `AdminUpdates` : `<Status>`/`<Job>` relayés vers les modèles partagés (paquets calaos, groupes noyau/système, progression), suivi d'un upgrade jusqu'à `doneReboot`, noms de paquets seulement avec une session et effacés à sa perte, `rate_limited` avec compte à rebours, reboot refusé pendant un upgrade, nouvel essai automatique des portes bloquées | `AdminRelay.cpp`, `AdminRelayBackend.cpp`, `AdminUpdates.cpp`, `UpdateModel.cpp`, `CalaosOsProtocol.cpp`, `CalaosConnection.cpp`, `TlsPinning.cpp`, `CalaosEventDecoder.cpp`, `HardwareUtils.cpp`, `Common.cpp`, `IOTypeRegistry.cpp` |
| `tst_boxtime` | Date et heure de la box sur le kiosque. `BoxTime` : décalage RFC 3339 du fuseau affiché, passage à l'heure d'été et d'hiver, heure sautée refusée, filtre des fuseaux. `CalaosOsAPI` et `BoxTimeManager` contre un faux démon (`FakeTimeDaemon.h`, routes du `docs/API.md` de calaos-container) : corps exact de chaque route, `ntp:false` et `time` dans la même requête, jeton en en-tête seulement, lecture à chaque ouverture et après chaque écriture, valeurs affichées venues de la seule relecture, réponse plus ancienne qu'une écriture ignorée, `404`, `400`, `409`, échec partiel `500`, box injoignable, `restart_required`, redémarrage de calaos-server et ses refus, liste des fuseaux avec nouvel essai | `BoxTime.cpp`, `BoxTimeManager.cpp`, `CalaosOsAPI.cpp`, `CalaosOsProtocol.cpp`, `NetworkRequest.cpp`, `TlsPinning.cpp`, `AsyncJobs.cpp`, `HardwareUtils.cpp`, `Common.cpp`, `IOTypeRegistry.cpp` |
| `tst_boxtimepage` | Qt Quick Test de `ConfigTimeView.qml` sur le vrai `BoxTimeManager` et le faux démon de `tst_boxtime` : état NTP, relecture à l'ouverture, rien d'affiché avant la première lecture, démon sans ces routes, liste filtrable, confirmations (coupure de NTP, écart de plus d'une heure, redémarrages), roues de date et heure, heure sautée, échec partiel, page tenue dans le plus petit kiosque. `BOXTIME_SHOTS_DIR=<dossier>` écrit les captures | mêmes sources que `tst_boxtime` |
| `tst_calaoseventdecoder` | `CalaosEventDecoder` : décodage pur des événements v2 (chaînes séparées par des espaces, percent-encodées) et v3 (objets JSON), des cartes `get_state` et des réponses de requête HTTP ; trames réelles des deux API, troncatures, types d'événements inconnus et charges utiles hors format, aucune ne devant faire crasher ni atteindre les modèles (T17) | `CalaosEventDecoder.cpp`, `Common.cpp` (+ `IOTypeRegistry.cpp`) |
| `tst_calaososprotocol` | `CalaosOsProtocol` : règles du protocole du daemon calaos-container, sans réseau. Enveloppe `/api/v2` (`{error,code,msg,data}`) face aux corps legacy (`{error,msg,output}`, objet brut), repli legacy sur un 404 **sans** enveloppe seulement (un 404 `not_found`/`package_not_found` vient d'un daemon v2), daemon v2 sans jobs (`not_found` sur les routes d'upgrade), `upgrade_in_progress` / `package_not_found` par code en v2 et par message (ou 409) en legacy, statut repos `""` des daemons 1.5 à 1.7, en-tête `Bearer`, parsing des jobs et états terminaux, `update/status` v2/legacy, et `UpgradeJobTracker` : seule la fin du job suivi termine, `upgrade_job` terminal arrivé avant le 202, lectures de statut ignorées pendant le POST, job perdu, adoption d'un job lancé ailleurs (CH1b) | `CalaosOsProtocol.cpp` |
| `tst_climatewidget` | Qt Quick Test, en rendu logiciel et en RHI (`QT_QUICK_BACKEND=rhi QSG_RHI_BACKEND=opengl`) : `ClimateLogic.js` (bornes, pas, plage et `min_range`, statut, consigne, boutons −/+), `ClimateDrafts.js` (commandes en vol : un champ n'est suivi que si la valeur envoyée diffère de l'attendue, la réponse de la commande k libère 0..k, une valeur que personne n'a envoyée libère tout, la réponse d'une commande numérotée, acceptée ou refusée, est rattachée à elle par son numéro : un refus ne termine que son vol), `ClimateModel` sur un vrai `IOBase` (commandes, brouillons, refus, décompte du lockout), l'item de liste, la bottom sheet mobile et le dialogue desktop avec leurs sheets / dialogues empilés, et le rendu en pixels du cadran. `CLIMATE_SHOTS_DIR=<dossier>` écrit les captures de chaque état. Les images de l'app (`img_desktop.qrc`) et les polices Roboto sont chargées pour que les captures ressemblent à l'app ; lance son propre moteur QML (offscreen) (MOB-2, MOB-3, MOB-4) | `RoomModel.cpp`, `Common.cpp`, `IOTypeRegistry.cpp` |
| `tst_common` | `Common::IOTypeToString` / `IOTypeFromString` (round-trip sur toutes les valeurs de `Common::IOType`, types stylés inclus), `Common::audioStatus*`, helpers de parsing défensif `Common::toIntSafe` / `toDoubleSafe` / `toLongLongSafe` sur `QString`, `QVariant` et `QJsonValue` — valeur rendue **et** politique de warning (T11) | `Common.cpp` |
| `tst_common` (suite) | `IOTypeRegistry` (T16) : couverture de l'enum, catégorie / `isLight` / `isDimmableLight` / `isMeasurement`, visibilité d'un IO dans une pièce, `styleName`, gui_types média, et table de vérité figée des 88 types du protocole historique (ex-`RoomModel::detectOldGuiType`) | `IOTypeRegistry.cpp` |
| `tst_eventlogmodel` | `EventLogIO::resolve()` : priorité input avant output dans la résolution d'un IO d'événement (T05) | `EventLogModel.h` (gabarit header-only) |
| `tst_eventlogsession` | Historique des événements sur une vraie session (EVLOG-1) : vrai `CalaosConnection` et vrai `EventLogModel`, câblés comme `Application` (`homeLoaded` → `refresh()`), contre un faux websocket calaos_server qui renvoie le `msg_id` reçu et un faux `api.php` en process. Page en vol perdue par un redémarrage du serveur, par le passage en arrière-plan iOS (`pause()`/`unpause()` + login), réponse `{"error": …}`, requête sans session, serveur muet (timeout de garde court injecté), transport HTTP avec la page au premier niveau : l'historique se recharge à chaque fois ; réponse en retard d'une requête abandonnée et réponse « un événement » (`loadEvent()`) ne terminent ni ne corrompent la page en vol ; un événement seul jamais répondu cesse de charger au timeout de garde ; logout et `loginFailed` remettent l'état à zéro (comme `Application`, qui appelle `reset()`), un autre home après un logout ne montre aucune ligne du précédent et repart de la page 0 ; `loadMore()` s'arrête à la dernière page, sans recharger une dernière page incomplète ni dupliquer un événement déjà affiché (dédoublonnage sur le champ `id` que renvoie calaos_server), y compris quand la vue demande la suite (`atYEnd` → `loadMore()`) pendant qu'un rafraîchissement remplace les lignes. `IOCache::searchInput/searchOutput` sont définis par le test (aucun IO) pour ne pas lier `RoomModel.cpp` | `CalaosConnection.cpp`, `CalaosEventDecoder.cpp`, `EventLogModel.cpp`, `HardwareUtils.cpp`, `Common.cpp`, `IOTypeRegistry.cpp` |
| `tst_homemodel` | `LightOnModel::clear()` vide bien `onCache` : pas de lumières fantômes après reconnexion (T06) | `HomeModel.cpp`, `RoomModel.cpp` (+ doublure locale `FakeConnection`, cf. T18 ; `HardwareUtils*` et `QT += widgets` retirés en T31) |
| `tst_httpreplies` | Durée de vie des réponses HTTP suivies par `CalaosConnection` (`reqReplies`, FIX-3) : une commande, une image de caméra ou une pochette audio qui échoue (500) puis un `logout()` / une reconnexion ne doivent pas toucher la réponse libérée. Construit avec AddressSanitizer (`CONFIG += sanitizer sanitize_address`) pour que l'use-after-free soit détecté à coup sûr | `CalaosConnection.cpp`, `CalaosEventDecoder.cpp`, `HardwareUtils.cpp`, `Common.cpp`, `IOTypeRegistry.cpp` |
| `tst_iobase` | `IOBase` seule : objet `device` des items climate (chargement, malformé, fusion de `io_changed`, MOB-1), `sendStringValue` / `sendIntValue` / `sendTrue` / `sendFalse` atteignent la connexion avec le bon sens d'IO, `getItemModel()` sans moteur QML, et garde-fou : `askStateText()` ne doit pas revenir dans le méta-objet (T31) | `RoomModel.cpp`, `Common.cpp`, `IOTypeRegistry.cpp` |
| `tst_mobilesettings` | Qt Quick Test (offscreen, rendu logiciel) de la vraie page `qml/mobile/SettingsView.qml`, propriétés de contexte remplacées par des doublures : champ « Nom de cet appareil » enregistré seulement si l'utilisateur l'a modifié, à chaque sortie de la page (retour, clavier ouvert, Terminé), laissé tel quel s'il est enregistré pendant la saisie, un renommage du système ne l'écrase pas pendant la saisie et le suit sinon, bouton effacer = retour au nom par défaut, abandonné par « Reset all data » (AP2b) ; page `qml/mobile/UpdatesView.qml` (CM1) : champ du mot de passe admin masqué, sans prédiction ni majuscule automatique, vidé à l'envoi et en quittant la page, déverrouillage bloqué pendant `rate_limited`, porte `https_required` avec le raccourci vers l'adresse de la box, `admin_not_set`, progression foyer visible verrouillé, reboot seulement après confirmation. Réutilise la doublure QuickFlux de `tst_climatewidget` | `Common.cpp`, `IOTypeRegistry.cpp` (module `Calaos` des feuilles) |
| `tst_httpapiversion` | `CalaosConnection::detectHttpApiV2()` (déclarée *header-only* dans `CalaosConnection.h`) : forme du payload `get_home` qui distingue l'API HTTP v2 de l'ancienne, règle déplacée depuis `RoomModel::load()` ; un payload muet (mode démo, home vide) doit laisser la valeur courante intacte (T18) | aucune (header seul) |
| `tst_pushnotif` | Notifications push partagées iOS/Android (FIX-3) : `PushRegistration` (`register_push` une fois par jeton et par session, renvoi sur rotation, rien hors session), `HardwareUtils` (tap différé hors connexion pris une seule fois, le dernier gagne, tap connecté ouvert tout de suite, `restorePendingNotif()` ; `deviceTokenChanged()` émis seulement sur changement ; `clearDeviceToken()` ; `describeToken()` tronqué ; nom d'appareil : valeur par défaut, assainissement — bords, caractères de contrôle, contrôles bidi et invisibles retirés (ZWJ/ZWNJ, sélecteurs de variante, étiquettes gardés), espaces fusionnés, plafond de 64 sans couper une paire de substitution ni une séquence emoji —, surcharge vide ou égale au nom de la plateforme = pas de surcharge), `DeferredNotifGate` seul et piloté comme `Application` (tap pendant chaque reconnexion ouvert, délai d'un login précédent sans effet, tap repris par le login suivant si la session retombe avant l'ouverture), un vrai `CalaosConnection` contre de faux serveurs HTTP (refusant en 400 toute requête sans `cn_user`/`cn_pass`, comme `api.php`), websocket et TLS en process (enregistrement authentifié au login, `device_name` en HTTP comme en websocket, renvoi immédiat et unique après un renommage dans les réglages (rien si le nom assaini est inchangé, au login suivant si déconnecté, à l'arrivée du jeton s'il manque, une seule fois pendant un login en vol, rien après un logout), jeton absent au login puis arrivé, après acceptation du login websocket, sur rotation, pendant un login en vol, après déconnexion, repli websocket → HTTP ; certificat auto-signé généré à la volée par la CLI `openssl`, cas sauté sinon), après un logout utilisateur ou « Reset all data » (`forgetPendingUserActions()`) ni tap différé ni Quick Action ne s'ouvrent au login suivant, alors que le bouton annuler (`logout()` simple) garde le tap (FIX-7), et aucun jeton ni mot de passe en clair dans les logs, dump debug du repli HTTP compris (construit en debug pour cela). Chaque fonction peut tourner seule | `CalaosConnection.cpp`, `CalaosEventDecoder.cpp`, `HardwareUtils.cpp`, `Common.cpp`, `IOTypeRegistry.cpp` |
| `tst_reconnectpolicy` | `ReconnectPolicy` (déclarée *header-only* dans `CalaosConnection.h`) : séquence de backoff 1/2/4/8/16/30/30 s et son plafond, bornes du jitter ±20 %, garde anti-double-login, inertie du second rapport d'échec, abandon borné sur credentials refusés vs retry infini sur erreur transitoire, tolérance du long-poll (T14) ; session fermée volontairement (`stop()`) jamais rouverte par `suspend()`/`resume()`, la découverte ou le timer, mise en arrière-plan (`pause()`/`unpause()`) qui bloque toute reconnexion automatique et garde l'intention (FIX-7) | aucune (header seul) |
| `tst_roomfiltermodel` | `RoomFilterModel::resetCache()` idempotent, partition sans doublon, `lessThan` robuste aux non-`IOBase` (T04) | `RoomFilterModel.cpp`, `RoomModel.cpp` (la doublure de lien `roommodel_stub.cpp` a disparu en T31) |
| `tst_sessionlifecycle` | Cycle de vie d'une session sur un vrai `CalaosConnection` contre un faux `api.php` en process, piloté comme `Application` (FIX-7) : logout puis réseau coupé/rétabli ou découverte ⇒ aucun login reçu par le serveur ; session active ⇒ reconnexion au retour du réseau ; login manuel après logout ; iOS : réseau qui bascule en arrière-plan sans login, retour au premier plan qui reconnecte une session active mais pas après un logout | `CalaosConnection.cpp`, `CalaosEventDecoder.cpp`, `HardwareUtils.cpp`, `Common.cpp`, `IOTypeRegistry.cpp` |
| `tst_updateclient` | Client de mise à jour du kiosque (vrais `CalaosOsAPI` et `UpdateManager`) contre un faux daemon calaos-container en process (HTTP et WebSocket d'événements sur un seul port 127.0.0.1, comme le daemon) : bearer dans l'en-tête `Authorization` partout, WebSocket compris, jamais `?token=` dans une URL ni le token dans les logs ; upgrade v2 : le 202 n'est pas la fin, seul l'état terminal du job l'est (événement `upgrade_job`, ou relecture de `update/status` quand l'événement est perdu), `reboot_required` du job, file de jobs d'un groupe, redémarrage du daemon pendant le job (reconnexion avec backoff, token relu), job lancé par un autre client adopté, `upgrade_job` terminal reçu avant le 202, job qui remplace le daemon (`daemon_restart` : la file attend le nouveau daemon, ou un délai de repli) ; POST v2 sans réponse : refusé = échec, coupé sans job = issue inconnue (jamais un succès), coupé après le départ du job = job suivi (preuve par `started_at` postérieur au POST, y compris par l'événement `running` reçu avant la coupure), file d'un groupe comprise, job d'un autre client plus ancien que le POST jamais crédité ; issue inconnue affichée sans « Upgrade failed » ; reprise après auto-mise à jour dès qu'une lecture de statut montre le nouveau daemon au repos ; instantané « idle » du nouveau daemon ignoré tant que la relecture d'après redémarrage n'a pas répondu ; reconnexion avec version inconnue qui relit `update/status` ; daemon ancien : repli legacy sur 404 texte, mis en cache, upgrade synchrone inchangé ; 404 enveloppé qui n'est pas un daemon ancien ; daemon v2 sans jobs ; 401 : token relu et un seul nouvel essai ; 409 `upgrade_in_progress` sur upgrade (suit le job de l'autre client sans s'attribuer ses paquets, y compris au milieu d'une file, où les paquets déjà installés gardent leur dialogue de redémarrage, comme après un POST coupé en milieu de file, sans « installed successfully » par-dessus l'avertissement ; un job lancé plus de 2 s après la coupure n'est jamais crédité), sur check et sur reboot (v2 et legacy) (CH1b) | `CalaosOsAPI.cpp`, `CalaosOsProtocol.cpp`, `NetworkRequest.cpp`, `AsyncJobs.cpp`, `UpdateManager.cpp`, `UpdateModel.cpp`, `HardwareUtils.cpp`, `Common.cpp`, `IOTypeRegistry.cpp` (+ quickflux) |
