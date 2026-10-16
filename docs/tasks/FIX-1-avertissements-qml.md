# FIX-1 — Avertissements QML à l'exécution (paramètres injectés, image d'animation)

| Champ | Valeur |
|---|---|
| Phase | P-FIX — Petits bugs constatés pendant P-AND |
| Taille | S |
| Bloqué par | — |
| Groupe de conflit | G-QML-MOBILE + G-QML-SHARED |
| Variantes | mobile (+ desktop pour `SharedComponents`) |

## Contexte

Relevé dans le logcat Android pendant AND-2 (2026-10-16) :

1. `qml/mobile/main.qml:128` — « Parameter "idx" is not declared » : le handler `onRoomClicked` utilise les paramètres de signal injectés (`idx`, `room_name`), comportement déprécié en Qt 6 (il cessera de fonctionner). D'autres handlers de `qml/mobile/` et `qml/SharedComponents/` utilisent probablement le même schéma.
2. `qml/SharedComponents/AnimatedIcon.qml:31` — « Cannot open: qrc:/img/icon_light_00-1.png » : `imageFilenameOn.arg(currentImage)` est évalué avec `currentImage == -1` (index hors plage au démarrage ou en fin d'animation).

## Fichiers

- `qml/mobile/*.qml`, `qml/SharedComponents/*.qml` (handlers concernés)
- `qml/SharedComponents/AnimatedIcon.qml`

## Implémentation

1. Remplacer toutes les utilisations de paramètres de signal injectés par des fonctions à paramètres explicites (`onRoomClicked: (idx, room_name) => { … }`) dans `qml/mobile/` et `qml/SharedComponents/` ; trouver les cas avec `qmllint` et une recherche ciblée.
2. `AnimatedIcon` : ne jamais construire un nom d'image avec un index hors plage (borne ou garde), sans changer l'animation visible.

## Critères d'acceptation

- Plus aucun « Parameter … is not declared » ni « Cannot open: qrc:/img/icon_light_00-1.png » dans le log à l'exécution (vérification au prochain passage sur appareil).
- `qmllint` sur les fichiers touchés : pas de nouvel avertissement, et les avertissements liés aux paramètres injectés disparaissent.
- Comportement inchangé : ouverture d'une pièce, animations d'icônes de lumière.
- Les variantes desktop et mobile utilisant `SharedComponents` ne régressent pas.
