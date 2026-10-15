# T30 — Nettoyage final : logs de debug, placeholders, formatters, collision de noms

| Champ | Valeur |
|---|---|
| Phase | P5 — Polish |
| Taille | S |
| Bloqué par | T28, T29 |
| Groupe de conflit | G-QML-SWEEP |
| Variantes | desktop + mobile |

## Contexte

Dette cosmétique résiduelle vérifiée :

1. **~20 `console.log`/`console.debug` de debug en production** : `qml/mobile/CameraListView.qml:35` (« component loaded »), `qml/desktop/main.qml:99,106,113,120` (« currentButton »), `main.qml:182`, `main.qml:344` (un TODO dans un log : « todo keyboard for item »), etc. — grep complet au démarrage.
2. **Placeholders lorem ipsum** : `qml/desktop/Notification.qml:107` (`text: "Test title"`) et `:120` (« Message lorem ipsum blabla … »).
3. **`showNetworkActivity()` vide** : `qml/desktop/main.qml:29-31` est un `//TODO` vide, pourtant appelé par le C++ (`HardwareUtils`). Implémenter (indicateur d'activité réseau simple) **ou** supprimer proprement la fonction ET son call-site C++ (`grep showNetworkActivity src/`).
4. **Trois formatters de temps redondants** : `qml/desktop/Utils.js:78` (`timeToString`), `Utils.js:107` (`time2string_digit`), `qml/SharedComponents/calaos.js` (`formatTime`) — règles et i18n différentes ; `timeToString` concatène des mots traduits un à un (fragile pour l'ordre des mots selon la langue).
5. **Collision de nom `Calaos`** : la lib JS `qml/SharedComponents/calaos.js` est enregistrée dans le `qmldir` sous `Calaos 1.0`, en collision avec le module C++ `Calaos` (`qmlRegisterType`). Des fichiers importent les deux (`desktop/RoomDetailView.qml:4,177`) — illisible.
6. Helpers de réflexion parent-chain dans `Utils.js:1-76` (`rootObject`, `findParent`, `sceneX/Y`…) : supprimer ceux sans call-site (grep d'abord).

## Fichiers

- Grep repo pour les `console.log`/`console.debug` (qml/ + widgets/)
- `qml/desktop/Notification.qml`, `qml/desktop/main.qml`, `qml/desktop/Utils.js`, `qml/SharedComponents/calaos.js`, `qml/SharedComponents/qmldir`
- Éventuellement un fichier src/ pour le call-site de `showNetworkActivity`

## Implémentation

1. Supprimer les logs de debug (conserver les `console.error`/logs d'erreur légitimes).
2. `Notification.qml` : remplacer les placeholders par des valeurs par défaut vides ou réalistes.
3. Trancher `showNetworkActivity` (implémentation minimale ou suppression complète des deux côtés).
4. Fusionner les formatters en UN (dans `calaos.js`), avec gestion i18n correcte (`qsTr` avec placeholders `%1` plutôt que concaténation de mots) ; migrer les call-sites ; supprimer les deux autres.
5. Renommer l'entrée qmldir de la lib JS (`Calaos 1.0 calaos.js` → `CalaosJs 1.0 calaos.js`) et mettre à jour tous les call-sites (`grep -rn "Calaos\." qml/` en distinguant lib JS vs module C++).
6. Supprimer les helpers Utils.js orphelins.

## Critères d'acceptation

- `grep -rn "console.log\|console.debug" qml/ widgets/` ≈ 0 (exceptions justifiées).
- `grep -rni "lorem" qml/` = 0.
- Un seul formatter de temps ; call-sites migrés.
- Plus de collision de nom `Calaos` (imports non ambigus).
- qmllint OK ; build + session rapide des deux variantes.

## Vérification

Build des deux variantes + parcours des vues touchées (notifications desktop, vues utilisant les formatters : audio, event log).
