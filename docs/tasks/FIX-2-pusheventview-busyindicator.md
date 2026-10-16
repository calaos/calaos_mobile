# FIX-2 — PushEventView : ReferenceError `control` dans le BusyIndicator

| Champ | Valeur |
|---|---|
| Phase | P-FIX — Petits bugs constatés pendant P-AND |
| Taille | S |
| Bloqué par | — |
| Groupe de conflit | G-QML-MOBILE |
| Variantes | mobile |

## Contexte

Relevé pendant AND-4 : à chaque ouverture de la vue d'événement poussé, `qml/mobile/PushEventView.qml:53` et `:56` lèvent « ReferenceError: control is not defined ». Le `contentItem` du `BusyIndicator` référence `control`, identifiant qui n'existe pas dans ce fichier (copié d'un exemple Qt Quick Controls). Le spinner ne tourne donc pas / ne se masque pas correctement.

## Fichiers

- `qml/mobile/PushEventView.qml`

## Implémentation

Donner un `id` au `BusyIndicator` et l'utiliser à la place de `control`, ou référencer `parent` de façon fiable ; vérifier que la même erreur n'existe pas ailleurs (`grep -rn 'control\.' qml/`).

## Critères d'acceptation

- Plus de ReferenceError à l'ouverture de la vue (vérification au prochain passage sur appareil, ex. `am start … --es event_uuid <uuid valide>`).
- Le spinner s'affiche pendant le chargement et disparaît ensuite.
- `qmllint` sans nouvel avertissement sur le fichier.
