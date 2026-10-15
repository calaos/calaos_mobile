# T13 — Timeouts HTTP et durcissement du long-poll

| Champ | Valeur |
|---|---|
| Phase | P2 — Robustesse réseau |
| Taille | S |
| Bloqué par | T12 (même fichier) |
| Groupe de conflit | G-CONN |
| Variantes | desktop + mobile |

## Contexte

**Aucune requête HTTP du projet n'a de timeout** (aucun `setTransferTimeout`, aucun QTimer de garde). Le seul mécanisme de vivacité est le ping/pong WebSocket (`CalaosConnection.cpp:174-187`). En fallback HTTP, le long-poll (`startJsonPolling`, lignes 655-737) peut pendre indéfiniment sans détection — l'UI semble connectée mais ne reçoit plus rien.

## Fichiers

- `src/CalaosConnection.cpp` (constructeur : les deux `QNetworkAccessManager` ; `startJsonPolling`)
- `src/NetworkRequest.cpp` (desktop)

## Implémentation

1. `QNetworkAccessManager::setTransferTimeout(30000)` sur `accessManager` et `accessManagerCam` dans le constructeur de CalaosConnection.
2. **Exception long-poll** : la requête `poll_listen` a une réponse légitimement lente (elle attend un événement). Lui poser un timeout **par requête** via `QNetworkRequest::setTransferTimeout(120000)` (valeur > au cycle de poll serveur), sinon le timeout global de 30 s tuerait le long-poll nominal et provoquerait des cycles logout/login parasites.
3. `NetworkRequest.cpp` : même `setTransferTimeout(30000)` sur son manager ou ses requêtes.

Note : à ce stade, un timeout déclenche `errorOccurred` → le comportement de retry/logout est celui existant ; il sera assoupli par T14 (tolérance aux échecs de poll + backoff). C'est pour ça que T13 précède T14.

## Critères d'acceptation

- Couper le réseau pendant une requête (hors long-poll) → erreur remontée en < 35 s au lieu de jamais.
- Le long-poll nominal (serveur calme, réponse lente légitime) n'est **pas** interrompu : session de 30 min sur serveur réel calme sans cycle logout/login parasite.
- Build desktop + mobile OK ; `make check` vert.

## Vérification

Test manuel avec un serveur réel : (a) débrancher le câble pendant une requête caméra → erreur < 35 s ; (b) session longue calme → stabilité.
