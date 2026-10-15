# T10 — Bundle gardes NaN / division par zéro en QML

| Champ | Valeur |
|---|---|
| Phase | P1 — Bugs critiques |
| Taille | S |
| Bloqué par | T03 |
| Groupe de conflit | fichiers disjoints de T07 ; bloque T27/T28 sur ces fichiers |
| Variantes | desktop + mobile |

## Contexte

Quatre divisions non gardées produisent des `NaN` (layouts cassés, warnings de binding) dans des états limites pourtant courants :

1. `qml/desktop/AudioPlayer.qml:210` — barre de progression : `elapsed / duration` clampé par `Math.min/Math.max`, mais quand les deux valent 0 (aucune piste), `0/0 = NaN` et le clamp retourne NaN → largeur NaN.
2. `qml/mobile/PushEventView.qml:85-87` — `ratio: sourceSize.width / sourceSize.height` puis `Layout.preferredHeight: width / ratio`. Avant chargement de l'image, `sourceSize` vaut 0×0 → NaN.
3. `qml/mobile/FavoritesEditView.qml:117` — `Math.floor((positionEnded - positionStarted)/parent.height)` : division par `parent.height` sans garde de zéro.
4. `qml/desktop/SpeedDialPage.qml:55` — `columns: Math.floor((parent.width - Units.dp(40)) / cellWidth)` peut valoir 0 sur parent étroit, ce qui effondre la largeur de la grille (ligne 53).

## Fichiers

- `qml/desktop/AudioPlayer.qml` (ligne 210)
- `qml/mobile/PushEventView.qml` (lignes 85-87)
- `qml/mobile/FavoritesEditView.qml` (ligne 117)
- `qml/desktop/SpeedDialPage.qml` (ligne 55)

## Implémentation

Pattern uniforme : `dénominateur > 0 ? a / b : valeurParDéfaut` :
- AudioPlayer : `duration > 0 ? ... : 0`.
- PushEventView : garde `sourceSize.height > 0` (ratio par défaut 1) ; s'assurer aussi que `ratio > 0` avant `width / ratio`.
- FavoritesEditView : garde `parent.height > 0` (résultat 0 sinon).
- SpeedDialPage : `columns: Math.max(1, Math.floor(...))`.

## Critères d'acceptation

- qmllint OK ; aucune régression visuelle sur les vues concernées.
- Player audio sans piste et vue push sans image : plus aucun warning de binding NaN dans la console, layout stable.

## Vérification

Lancer les deux variantes ; sur desktop ouvrir le player audio sans musique en cours ; sur mobile ouvrir une notification push sans image.
