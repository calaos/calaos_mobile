# T18 — Découpler RoomModel → CalaosConnection (updateHttpApiV2)

| Champ | Valeur |
|---|---|
| Phase | P3 — Découplage C++ |
| Taille | S |
| Bloqué par | T16, T17 |
| Groupe de conflit | G-MODELS + G-CONN |
| Variantes | desktop + mobile |

## Contexte

`RoomModel::load` **écrit de l'état protocolaire dans la couche transport** : `connection->updateHttpApiV2(true/false)` (`src/RoomModel.cpp:113,119`) selon la forme du payload reçu. La dépendance CalaosConnection ↔ RoomModel est donc bidirectionnelle (la connexion émet des événements vers IOBase ; le modèle règle la version d'API de la connexion). C'est le dernier couplage inverse après T16/T17.

## Fichiers

- `src/RoomModel.cpp` (lignes 113, 119)
- `src/CalaosConnection.cpp/.h`

## Implémentation

Option privilégiée : **détecter la version d'API dans CalaosConnection elle-même** — elle possède déjà le JSON de la réponse `get_home` avant de le transmettre au modèle ; le critère de détection utilisé par RoomModel (forme du payload) peut être évalué au même endroit, dans la couche qui possède le flag.

Option de repli (si le JSON n'est pas inspectable proprement côté connexion) : RoomModel émet un signal `apiVersionDetected(bool v2)` que `Application` connecte à `CalaosConnection::updateHttpApiV2` — le modèle ne tient plus de pointeur vers la connexion pour ça.

## Critères d'acceptation

- `RoomModel` n'appelle plus `updateHttpApiV2` (grep = 0 dans RoomModel.cpp).
- Login fonctionnel contre un serveur v1/v2 (HTTP) et v3 (WebSocket) — au minimum : demo.calaos.fr + un serveur réel.
- `make check` vert ; build desktop + mobile.

## Vérification

Session manuelle : login WebSocket (chemin nominal) puis forcer le fallback HTTP (bloquer le port WS ou utiliser un serveur ancien) — les deux chemins chargent la maison.
