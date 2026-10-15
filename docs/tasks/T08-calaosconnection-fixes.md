# T08 — CalaosConnection : correctifs ciblés pré-refactoring

| Champ | Valeur |
|---|---|
| Phase | P1 — Bugs critiques |
| Taille | M |
| Bloqué par | T02 |
| Groupe de conflit | G-CONN |
| Variantes | desktop + mobile |

## Contexte

`CalaosConnection.cpp` (924 lignes) sera lourdement retouché en P2 (TLS, timeouts, reconnexion) puis P3 (extraction du décodeur). Stratégie « fix first, split later » : on purge d'abord les crashs et incohérences par des correctifs chirurgicaux, pour que les gros tickets partent d'une base saine.

Quatre défauts vérifiés :

1. **Crash possible sur événement malformé** — `CalaosConnection.cpp:769-771` : la branche `audio_status` accède à `spl.at(1)` et `spl.at(2)` **sans garde de taille** (contrairement à la branche `audio_volume` juste au-dessus qui vérifie `spl.count() < 4`). Un événement `"audio_status foo"` (2 tokens) → `QList::at` hors bornes.
2. **Timer utilisé avant création** — dans `onWsConnected`, le lambda `pong` (ligne ~171) appelle `wsPingTimeout->start()` mais `wsPingTimeout` n'est créé qu'à la ligne ~181, après le `connect`. Fonctionne par chance (aucun pong ne peut arriver entre les deux) ; fragile à toute réorganisation.
3. **Validation JSON des frames WebSocket plus faible que les chemins HTTP** — `onWsTextMessageReceived` (~ligne 826) parse sans exploiter `QJsonParseError` ni vérifier le type d'objet, alors que `loginFinished` (288), `requestFinished` (348) et le poll (701) le font.
4. **Credentials en clair dans les logs** — les URLs contenant `cn_user`/`cn_pass` (URLs caméra v1 ~ligne 585-589, `getNotifPictureUrl` ~ligne 919-923) sont imprimées par des `qDebug() << ... << url` (ex. lignes 274, 340, 423, 447) → mots de passe en clair dans les logs.

## Fichiers

- `src/CalaosConnection.cpp` uniquement (et `.h` si besoin pour un helper privé)

## Implémentation

1. Branche `audio_status` : garde `if (spl.count() > 2)` avant les accès (aligner sur le style des branches voisines) ; sinon `qWarning` + return.
2. Créer `wsPingTimer` **et** `wsPingTimeout` avant la connexion du lambda `pong` dans `onWsConnected`.
3. Aligner la validation des frames WS sur le pattern HTTP : vérifier `err.error == QJsonParseError::NoError` et `jdoc.isObject()` avant usage ; `qWarning` + return sinon.
4. Ajouter un helper privé `static QString redactUrl(const QUrl &)` qui masque les valeurs des paramètres `cn_user`, `cn_pass`, `u`, `p` (→ `***`), et l'utiliser dans **tous** les `qDebug`/`qWarning` qui impriment une URL. Ne pas changer les URLs réellement envoyées (le passage GET→POST est traité en T12).

## Critères d'acceptation

- Un événement `audio_status` tronqué injecté (test manuel ou unitaire si faisable) ne crash pas.
- Grep sur une session de logs complète (login + caméras + notification) : `cn_pass` et le mot de passe réel n'apparaissent **jamais**.
- Build desktop + mobile OK ; `make check` vert.

## Vérification

Session complète contre un serveur réel ou demo.calaos.fr : login, événements, caméras. Comparer les logs avant/après pour la rédaction des credentials.
