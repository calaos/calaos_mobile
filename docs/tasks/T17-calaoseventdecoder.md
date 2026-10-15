# T17 — Extraction du décodeur d'événements (CalaosConnection, étape 1)

| Champ | Valeur |
|---|---|
| Phase | P3 — Découplage C++ |
| Taille | L |
| Bloqué par | T14 (dernier ticket fonctionnel sur CalaosConnection) |
| Groupe de conflit | G-CONN |
| Variantes | desktop + mobile |

## Contexte

`CalaosConnection.cpp` (924 lignes) est une god class : transport WS + HTTP + long-poll, auth, parsing d'événements v1/v2/v3, téléchargement de blobs caméra/audio. Le parsing d'événements est le cœur non testé le plus critique (deux `//TODO all other event types` aux lignes 780 et 823 attestent d'un parseur incomplet).

Ce ticket extrait **uniquement le décodage d'événements** vers des fonctions pures testables. Le split transport complet (classes WsTransport/HttpTransport) est volontairement **différé** (rendement/risque défavorable avant la migration CMake — voir BOARD.md, hors périmètre).

## Fichiers

- **Nouveaux** : `src/CalaosEventDecoder.h`, `src/CalaosEventDecoder.cpp` → **à enregistrer dans `calaos.pri`**
- `src/CalaosConnection.cpp/.h` (les parseurs délèguent)
- **Nouveau test** : `tests/tst_eventdecoder/` (+ `tests/tests.pro`) avec fixtures JSON dans `tests/data/`

## Implémentation

1. API pure, sans état ni QObject :
   ```cpp
   struct DecodedEvent {
       enum class Type { IoChanged, RoomChanged, AudioStatus, AudioVolume, ScenarioChange, TouchScreenCamera, PushNotification, Unknown, ... };
       Type type;
       QVariantMap payload;   // ou champs typés par type d'événement
       bool valid;
   };
   static DecodedEvent CalaosEventDecoder::decode(const QJsonObject &event, ApiVersion v);
   ```
   Couvrir les trois versions de protocole gérées aujourd'hui (chemins v1 string-split type `audio_status`, v2 et v3 JSON — reprendre la logique existante à l'identique, y compris les gardes ajoutés par T08).
2. `CalaosConnection` ne fait plus que : recevoir le message → `decode()` → `switch` sur `DecodedEvent::Type` → émettre ses signaux existants (signatures de signaux **inchangées** pour ne pas toucher les modèles).
3. Fixtures de test : capturer/reconstituer des JSON réels d'événements (io state change input/output, audio_status, event_log v1/v2/v3) + les cas malformés couverts par T08 (troncatures, types inattendus, JSON invalide) → `decode()` retourne `valid=false` sans crash.

## Critères d'acceptation

- Tests fixtures verts, y compris tous les cas malformés.
- `CalaosConnection.cpp` réduit d'au moins ~200 lignes.
- Signatures des signaux publics de CalaosConnection inchangées (aucune modification dans les modèles).
- Comportement runtime inchangé : session complète desktop + mobile contre serveur réel (événements IO, audio, caméra, notifications reçus normalement).
- `make check` vert.

## Vérification

`make check` + session manuelle : déclencher des événements de chaque type (allumer une lumière, changer le volume audio, lancer un scénario) et vérifier leur prise en compte par l'UI.
