# T07 — Bundle bugs QML : enums inexistants, iconState, densité double, action QuickFlux

| Champ | Valeur |
|---|---|
| Phase | P1 — Bugs critiques |
| Taille | S |
| Bloqué par | T03 |
| Groupe de conflit | G-QML-SHARED (+ `qml/desktop/main.qml` : bloque T25/T27/T28 sur ce fichier) |
| Variantes | desktop + mobile |

## Contexte

Cinq bindings morts vérifiés, invisibles à l'exécution car QML évalue silencieusement `undefined` :

1. **Alerte gaz morte** — `qml/SharedComponents/IOSwitch.qml:47` teste `Common.GasSensor`, mais l'enum C++ s'appelle **`GasLeakSensor`** (`src/Common.h:70`). La comparaison vaut toujours false : un capteur de gaz n'entre jamais dans la branche alerte rouge/clignotante (lignes 45-53) et tombe dans le rendu générique On/Off.
2. **Mauvais delegate SwitchLong** — `qml/SharedComponents/ItemListView.qml:86` teste `Common.Switch_long`, l'enum réel est **`SwitchLong`** (`src/Common.h:51`). Un IO SwitchLong tombe sur le `default_delegate` (texte nu, sans contrôles). Noter que `IOSwitch.qml:27` utilise le bon nom — preuve de dérive copy-paste.
3. **Boiler/Heater toujours bleus** — `qml/SharedComponents/IOBoiler.qml:25` et `IOHeater.qml:26` : `color: icon.iconState ? "#ffda5a" : "#3ab4d7"`. Ici `icon` est un `IconItem` (simple `Image`, **aucune propriété `iconState`**) ; l'expression vaut toujours false. Copié depuis `IOLight.qml:30` où `icon` est un `AnimatedIcon` (qui possède `iconState`).
4. **Densité appliquée deux fois** — `qml/desktop/main.qml:13-14` : `width: Units.dp(1024) * calaosApp.density`. `Units.dp()` multiplie déjà par la densité (`Units.cachedValue` est bindé à `calaosApp.density` en `main.qml:156-158`) → taille = 1024 × densité².
5. **Wrapper QuickFlux mort-et-faux** — `qml/quickflux/AppActions.qml:55-58` : `openEventPushViewerUuid(uuid)` dispatche `ActionTypes.openEventPushViewer` (sans passer l'uuid au bon listener) au lieu de `ActionTypes.openEventPushViewerUuid`. Le listener qui lit `notifUuid` est sur `openEventPushViewerUuid` (`qml/mobile/main.qml:366-374`). Ça « marche » aujourd'hui uniquement parce que le C++ dispatche la chaîne brute directement (`src/Application.cpp:456,730`), court-circuitant le wrapper.

## Fichiers

- `qml/SharedComponents/IOSwitch.qml:47`
- `qml/SharedComponents/ItemListView.qml:86`
- `qml/SharedComponents/IOBoiler.qml`, `qml/SharedComponents/IOHeater.qml`
- `qml/desktop/main.qml:13-14` (**ne pas toucher** au reste du fichier — le TODO `showNetworkActivity` ligne 29 est traité en T30)
- `qml/quickflux/AppActions.qml:55-58`

## Implémentation

1. `IOSwitch.qml:47` : `Common.GasSensor` → `Common.GasLeakSensor`.
2. `ItemListView.qml:86` : `Common.Switch_long` → `Common.SwitchLong`.
3. `IOBoiler.qml` / `IOHeater.qml` : répliquer le mécanisme de couleur d'état d'`IOLight.qml` (icône `AnimatedIcon` avec `iconState`, ou binding direct sur `modelData.stateBool`). Choisir la solution la plus proche du comportement d'IOLight ; ne pas restructurer davantage (la fusion des widgets binaires est T23).
4. `desktop/main.qml:13-14` : supprimer le facteur `* calaosApp.density` (garder `Units.dp(1024)` / `Units.dp(600)`).
5. `AppActions.qml:55-58` : corriger le type dispatché en `ActionTypes.openEventPushViewerUuid` (moindre risque ; le wrapper devient correct même s'il reste inutilisé par le C++).

## Critères d'acceptation

- Build + qmllint OK sur les deux variantes.
- Vérif visuelle : (a) un capteur gaz en état détecté affiche l'UI d'alerte rouge clignotante ; (b) un IO SwitchLong a son delegate switch ; (c) Boiler/Heater passent jaune quand actifs ; (d) la fenêtre desktop a la bonne taille sur écran hidpi (density > 1).
- Grep : `grep -rn "GasSensor\b\|Switch_long" qml/` = 0 occurrence.

## Vérification

`make check` + lancement desktop (mode démo si besoin) pour la taille de fenêtre ; les points capteurs peuvent être vérifiés sur maison de démo ou par relecture des bindings.
