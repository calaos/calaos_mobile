# CM1 — Mises à jour de la box depuis l'application mobile

| Champ | Valeur |
|---|---|
| Phase | P-ADMIN — Administration de la box, phase 3 (`docs/admin/PLAN.md` de calaos_base) |
| Taille | L |
| Bloqué par | CM2, CM2b (épinglage TLS) |
| Groupe de conflit | G-CONN + G-APP + G-QML-MOBILE |
| Variantes | mobile (iOS, Android) ; desktop : sources partagées compilées, comportement du kiosque inchangé |

## Contexte

La webapp (WA1) vérifie et installe les mises à jour et redémarre la box avec le mot de passe admin, par le relais admin de calaos_server (`admin_unlock`, `admin`, `admin_event`). Le téléphone doit faire la même chose, en https, sur la WebSocket foyer déjà ouverte (identifiants `cn_user`/`cn_pass`, TLS épinglé par CM2). Pas de bootstrap, de changement de mot de passe, d'approbateur ni de console depuis le mobile. Contrat : section « Administration » de `docs/08_http_api.md` de calaos_base ; libellés et noms des portes : store `admin` et `UpdatesPanel` de calaos-web-app.

## Fichiers

- `src/AdminRelay.h|cpp` (nouveau, partagé), `src/AdminRelayBackend.h|cpp` (nouveau), `src/AdminUpdates.h|cpp` (nouveau), `src/UpdateBackend.h` (nouveau)
- `src/CalaosOsAPI.h` (implémente `UpdateBackend`), `src/CalaosOsProtocol.h|cpp` (`finished_at`, `package` du job)
- `src/CalaosConnection.h|cpp` (`AdminTransport`, routage des réponses admin et de `admin_event`, signal `tlsTrustAccepted`)
- `src/Application.h|cpp` (câblage mobile)
- `qml/mobile/UpdatesView.qml` (nouveau), `qml/mobile/SettingsView.qml`, `qml/mobile/main.qml`, `qml_mobile.qrc`
- `calaos.pri`, `desktop.pro`, `cmake/CalaosHomeSources.cmake`
- `lang/calaos_fr.ts|qm`, `lang/calaos_de.ts|qm`
- `tests/tst_adminrelay/` (nouveau), `tests/tst_mobilesettings/`, `tests/tests.pro`, `tests/README.md`
- `doc/mobile.md`, `AGENTS.md`

## Implémentation

- `AdminRelay` sur `CalaosConnection` (interface `AdminTransport`, WebSocket seulement) : `unlock(pass)`, `request(action, params)` avec corrélation par `msg_id`, délai de 5 s (125 s pour `update_check`), `session()`, `expiresIn()`, `isUnlocked()`, `handleReply()`, `handleEvent()`. Portes, comme le store de la webapp : `locked`, `unlocked`, `https_required`, `admin_not_set`, `unavailable`, `too_old`, `unreachable`, `old_server` (plus `idle` et `probing` avant la première réponse).
- Session admin : en mémoire seulement, jamais enregistrée ni journalisée ; 15 min absolues au plus ; perdue avec le socket, à la déconnexion, à l'expiration (revérifiée au retour au premier plan), sur `bad_session`, sur `admin_event` `admin_set` / `admin_reset` et quand l'utilisateur accepte une nouvelle clé de certificat. Aucun `admin_unlock` tant que `hasPendingTlsTrust()`.
- `UpdateBackend` : interface des appels de mise à jour. `CalaosOsAPI` l'implémente (kiosque, daemon en direct, inchangé) ; `AdminRelayBackend` aussi (mobile : `update_status`, `update_check`, `update_available`, `update_upgrade_all`, `update_upgrade {package}`, `system_reboot`, `system_info`, événements `admin_event upgrade_progress`). Les modèles (`UpdatePackagesModel`, `UpdateGroupsModel`, `UpdateProgressModel`), `UpdateGrouping` et `CalaosOsProtocol` deviennent des sources partagées. `UpdateManager` reste au kiosque (`CALAOS_DESKTOP`) avec sa logique daemon (routes legacy, liste installée, flux d'événements du daemon, redémarrage du daemon).
- `AdminUpdates` (propriété de contexte `boxUpdates`) : le parcours de l'`UpdatesPanel` de la webapp. `update_status` sert de sonde et fait foi après une reconnexion ; suivi d'un job par événements et relecture toutes les 5 s ; phases `unknown`, `upToDate`, `available`, `upgrading`, `done`, `doneReboot`, `failed` ; nouvel essai automatique des portes `unavailable` (5 s pendant 2 min puis 60 s), `too_old` et `unreachable` (attente croissante), « Redémarrez la box » après 2 min de `too_old` ; `rate_limited` avec compte à rebours.
- Libellés : `https_required` → « Les mises à jour demandent une connexion https à la box », avec le raccourci « Changer l'adresse de la box » (retour à l'écran de connexion) ; `admin_not_set` → « Définissez d'abord le mot de passe admin depuis la webapp » ; `admin_unavailable` / `daemon_*` / serveur ancien → textes de la webapp ; `bad_session` → nouvelle demande du mot de passe, sans message.
- QML : Réglages → « Mises à jour de la box » ; carte de déverrouillage puis carte des mises à jour (titre, liste des paquets calaos et des groupes, « Tout mettre à jour », barre de progression, « Vérifier les mises à jour ») et carte « Redémarrer la box » avec confirmation.

### Ce qui est partagé avec le kiosque

- Partagé (sources communes) : `UpdateBackend` (interface), `UpdateModel` (les trois modèles), `UpdateGrouping`, `CalaosOsProtocol`. `CalaosOsAPI` implémente `UpdateBackend` mais reste propre au kiosque.
- `UpdateManager` n'a pas été porté sur l'interface (écart assumé au design) : il dépend de ce que seul le daemon offre (liste des paquets installés, routes legacy synchrones, flux `/events` du daemon avec `upgrade_job`, détection d'un daemon qui se met à jour lui-même, files de POST par groupe, vérifications automatiques avec le dialogue). Le porter aurait mis en jeu le comportement du kiosque sans rien apporter au mobile.
- `AdminUpdates` est donc un second orchestrateur sur les modèles partagés : ses phases suivent l'`UpdatesPanel` de la webapp, l'alimentation des modèles reprend celle d'`UpdateManager`. Code jumeau à modifier ensemble : `AdminUpdates::applyAvailable()` ↔ `UpdateManager::applyAvailable()`, `AdminUpdates::applyProgress()` ↔ la partie paquets d'`UpdateManager::applyStatusSnapshot()`, traitement des trames terminales/idle (`held()`/`onProgressEvent()` ↔ `applyStatusSnapshot()`).
- Correspondance des portes avec la webapp : `admin_not_set` = `bootstrap`/`household`, `too_old` = `activating`, `websocket_required` propre au mobile (repli HTTP), les autres ont le même nom.

## Sécurité

- SR-21 : champ du mot de passe admin en `TextInput.Password`, `inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase`, jamais lié à un réglage ; vidé à l'envoi, en quittant la page et quand l'application passe en arrière-plan. Ni le mot de passe ni la session dans les journaux.
- SR-22 : nouvelle clé de certificat acceptée → session abandonnée, mot de passe redemandé ; aucun message admin pendant une question de certificat.
- Le mot de passe ne part jamais en clair ni vers une autre box : `admin_unlock` refusé sans envoi sur un socket non `wss` (porte `https_required`), sur le repli HTTP, et tant qu'aucune réponse admin n'est arrivée sur le socket courant ; toute perte du socket remet la porte à `idle`. Les requêtes faites avec la session échouent (`session_lost`) quand elle se termine.
- SR-23 : `update_status` sans session = vue foyer filtrée ; les noms de paquets ne s'affichent qu'une fois déverrouillé et sont effacés à la perte de la session. Le kiosque garde `CalaosOsAPI` en direct sur le daemon.

## Hors périmètre

- Bootstrap et changement du mot de passe admin, approbateurs, console : webapp uniquement.
- Mise à jour d'un seul paquet depuis l'interface (l'appel `update_upgrade` existe dans le backend, l'écran ne propose que « Tout mettre à jour », comme la webapp).
- Réécriture automatique `http://` → `https://` de l'hôte enregistré (CI1 le fait pour l'installer) : l'utilisateur corrige l'adresse sur l'écran de connexion.

## Critères d'acceptation

- `tst_adminrelay` : corrélation et portes contre un faux transport et contre un faux calaos_server WebSocket (déverrouillage réussi, `bad_password`, `https_required`, délai dépassé → `old_server`), cycle de vie de la session, correspondance `<Status>`/`<Job>` → modèles partagés.
- `tst_mobilesettings` (`tst_updatesview.qml`) : champ du mot de passe (SR-21), confirmation du reboot, portes.
- Non-régression du kiosque : `tst_updateclient` et `tst_calaososprotocol` verts ; `make check` vert hormis FIX-13.
- Builds : `desktop.pro`, `tools/build_ios.sh Debug` (appareil), `tools/build_android.sh Debug` ; `tools/check_sources.sh` propre.
- Manuel (Raoul) : mise à jour depuis le téléphone, progression qui survit à un redémarrage de calaos-server, bouton de redémarrage.
