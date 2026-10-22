# EVLOG-1 — Historique des événements figé après une requête perdue

| Champ | Valeur |
|---|---|
| Phase | P-FIX — Petits bugs constatés après P-AND |
| Taille | M |
| Bloqué par | — |
| Groupe de conflit | G-CONN + G-MODELS |
| Variantes | mobile (iOS, Android) + desktop (modèle partagé) |

## Contexte

Signalé par Raoul le 2026-10-22 : sur iOS, l'historique des événements est resté figé au 21/10 17:23:15 après la mise à jour de la box (redémarrage de calaos_server vers 17:32) ; Android affichait les événements du jour ; tuer et relancer l'app iOS a réglé le problème. Le serveur répond correctement.

Enquête du 2026-10-22 (reproduite sur simulateur iOS 27 et dans un harnais Qt hôte) : `EventLogModel::loading` est un verrou à sens unique. Il est posé par `load()` (`src/EventLogModel.cpp:36-44`) et levé uniquement par une réponse de page réussie (`:72`). Rien ne le réinitialise sur perte de transport, logout, `loginFailed` ou timeout. Dès qu'une requête est perdue, `refresh()` (`:85`, appelé par `Application::homeLoaded`), le tirer-pour-rafraîchir et `loadMore()` (`:49`) deviennent sans effet, et les anciennes lignes restent affichées jusqu'à la mort du processus.

Chemins de perte identifiés :
1. **iOS** (depuis FIX-7) : chaque retour au premier plan fait login → `homeLoaded` → `refresh()` ; passer en arrière-plan avant la réponse appelle `pause()` → `resetTransport()` (`Application.cpp:69-75`, `CalaosConnection.cpp:249-256`) → réponse perdue.
2. Redémarrage de calaos_server / haproxy ou timeout de ping pendant une requête (`connectionLost()` → `resetTransport()`, `CalaosConnection.cpp:155-218`) — iOS et Android.
3. Réponse d'erreur `{"error": …}` du serveur (SQL, page hors limites) : `logEventLoaded()` sort tôt sans lever le verrou (`EventLogModel.cpp:56-57`).
4. Session en repli HTTP : `CalaosEventDecoder::decodeQueryAnswer` (`src/CalaosEventDecoder.cpp:294-298`) émet `answer["data"]` (vide) alors que la réponse HTTP eventlog est au premier niveau (`{total_page, total_count, page, per_page, events}`) → l'historique ne charge jamais en HTTP ; `tests/tst_calaoseventdecoder` (~`:630-647`) valide la mauvaise forme. Bug présent depuis Qt 5.
5. `load()` sans transport : `sendWebsocket`/`sendHttp` abandonnent en silence (`CalaosConnection.cpp:787`, `:804`) mais le verrou est déjà posé.
6. Erreur HTTP ou JSON illisible dans `requestFinished()` (`:692-707`) : rien n'est émis.
7. Le serveur peut ne jamais répondre (bug calaos_base, traité à part).

Les requêtes eventlog WS partent toutes avec `msg_id = "user_cmd_json"` (`CalaosConnection.cpp:862`), sans corrélation ; le serveur renvoie le `msg_id` reçu (`calaos_base JsonApiHandlerWS.cpp`). Les réponses de page et les réponses « un seul événement » (`EventLogItem`, par uuid) partagent le même signal `logEventLoaded`.

Effet de bord à corriger aussi : `loadMore()` calcule la page à partir de `rowCount()/50`, ce qui recharge et duplique une dernière page incomplète.

Matériel de l'enquête (hors repo) : faux serveur WS/HTTP `/tmp/evlog_fake/fake_calaos_ws.py` (+ `run.sh`, logs), harnais hôte `/tmp/evlog_host/tst_evlog.cpp`, instrumentation `/tmp/evlog_instrumentation.diff`.

## Fichiers

- `src/EventLogModel.cpp|h`, `src/CalaosConnection.cpp|h`, `src/CalaosEventDecoder.cpp|h`, `src/Application.cpp`
- QML de l'historique (mobile et desktop) seulement si une API change
- `tests/` (nouveau test de session eventlog, `tst_calaoseventdecoder`, `tst_eventlogmodel` s'il existe)

## Implémentation

1. **Une fin garantie pour chaque requête de page** : `msg_id` unique par requête (ex. `eventlog_page:<génération>:<page>`), table des requêtes en attente dans `CalaosConnection`, signal de fin distinct pour les pages (succès / échec), `resetTransport()` et les erreurs HTTP/JSON signalent l'échec de toutes les requêtes en attente, `sendJson` renvoie `false` sans transport.
2. **`EventLogModel` robuste** : numéro de génération + timeout de garde (20–30 s, injectable pour les tests) à la place du booléen ; remise à zéro quand la connexion quitte l'état connecté, au logout et sur `loginFailed` ; `refresh()` lance toujours une nouvelle génération et ignore les réponses en retard ; une réponse `{"error": …}` termine la requête ; réponses de page et réponses « un événement » séparées.
3. **Décodeur HTTP** : émettre la réponse de premier niveau (repli sur `"data"` si présent) ; corriger le test.
4. **`loadMore()`** : numéro de page suivi explicitement, pas de doublons.
5. Desktop : garder le comportement (le modèle est partagé) ; vérifier la vue desktop de l'historique.

## Critères d'acceptation

Tests Qt (harnais `tests/`, vrai `CalaosConnection` contre un faux serveur WS/HTTP en processus, sur le modèle du harnais d'enquête) :
- requête perdue par redémarrage du serveur → après reconnexion, `homeLoaded` renvoie une requête et le modèle se remplit ;
- `pause()`/`unpause()` + login avec requête en attente → rechargement ;
- réponse d'erreur → le `refresh()` suivant envoie une requête ;
- `load()` sans transport → pas de blocage ;
- serveur muet → le timeout libère (timeout court injecté) ;
- transport HTTP → une page de premier niveau se charge ;
- réponse en retard d'une ancienne génération ou réponse « un événement » → ne termine ni ne corrompt une requête de page ;
- logout / `loginFailed` → état remis à zéro ;
- `loadMore` → pas de doublons sur une dernière page incomplète ;
- `tst_calaoseventdecoder` avec la vraie forme HTTP.
Ces tests échouent sur le code actuel. Builds Android, iOS et desktop OK. Sur simulateur iOS : scénarios A (redémarrage serveur) et B (arrière-plan) de l'enquête → l'historique se rafraîchit.
