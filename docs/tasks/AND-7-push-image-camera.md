# AND-7 — Image de caméra dans les notifications push Android

| Champ | Valeur |
|---|---|
| Phase | P-AND — Android Qt 6 |
| Taille | L |
| Bloqué par | AND-4 |
| Groupe de conflit | G-ANDROID |
| Variantes | mobile (Android) |

## Contexte

Sur iOS, l'extension `ios/PushNotif/NotificationService.m` enrichit chaque push : si le payload contient `event_uuid`, elle
1. lit l'hôte (`calaos.hostname` dans l'App Group, normalisé : `ws(s)://` → `http(s)://`, sinon `https://<host>/api`) et les identifiants (`calaos-cn-user` / `calaos-cn-pass` dans le keychain partagé) ;
2. appelle `<host>?cn_user=…&cn_pass=…&action=eventlog&uuid=<event_uuid>` → JSON avec `pic_uid` ;
3. télécharge `<host>?cn_user=…&cn_pass=…&action=event_picture&pic_uid=<pic_uid>` (GIF) ;
4. attache l'image à la notification ; en cas d'échec, la notification d'origine est affichée telle quelle.

Android n'a jamais eu cette fonctionnalité. Équivalent : `CalaosMessagingService.onMessageReceived` (AND-4) affiche une notification `BigPictureStyle`.

Contraintes Android :
- `onMessageReceived` n'est appelé app en arrière-plan **que pour les messages `data`**. Si `push.calaos.fr` envoie un message `notification` pour Android (cf. rapport AND-4), il faut un changement côté relais (hors repo) : le signaler et le documenter, implémenter côté app quand même.
- `onMessageReceived` a ~10 s : téléchargement dans un job `WorkManager` expédié, notification simple affichée immédiatement puis mise à jour avec l'image (même ID).
- Le service tourne **sans Qt chargé** : l'hôte et les identifiants doivent être accessibles en Java. Aujourd'hui ils sont en clair dans `QSettings` (cf. BOARD, chantier Keystore).

## Fichiers

- `android/src/fr/calaos/calaoshome/CalaosMessagingService.java`
- **Nouveaux** : worker de téléchargement, stockage sécurisé des identifiants (Java)
- `android/HardwareUtils_Android.cpp|h` (surcharges `saveAuthKeychain` / `loadAuthKeychain` / `resetAuthKeychain` / `setConfigOption`, comme `ios/HardwareUtils.mm`)
- `android/build.gradle` (WorkManager)

## Implémentation

1. **Stockage** : `HardwareUtilsAndroid` surcharge les méthodes keychain/config pour écrire hôte et identifiants côté Java, chiffrés avec une clé AES de l'**Android Keystore** (pas `EncryptedSharedPreferences`, déprécié). Migration transparente des identifiants existants depuis `QSettings`, puis suppression de la copie en clair. `resetAuthKeychain` (logout) efface tout.
2. **Réception** : si `event_uuid` est présent, afficher la notification texte tout de suite, puis lancer le worker.
3. **Worker** : mêmes deux requêtes que iOS, timeouts courts, TLS validé par le système (comme `NSURLSession` sur iOS — pas d'acceptation silencieuse des certificats auto-signés en arrière-plan avec des identifiants dans l'URL ; documenter la limite pour les serveurs auto-signés). Décodage de la première image du GIF, réduction de taille raisonnable, mise à jour de la notification en `BigPictureStyle` (+ `largeIcon`).
4. Aucun log contenant l'URL complète, les identifiants ou l'image.
5. Échec à n'importe quelle étape → la notification texte reste, sans erreur visible.

## Critères d'acceptation

- Un événement caméra sur le serveur Calaos produit, sur le téléphone app tuée, une notification avec l'image de la caméra.
- Même résultat app en arrière-plan et au premier plan.
- Serveur injoignable / mauvais identifiants / pas de `pic_uid` : notification texte seule, pas de crash.
- Après logout, plus aucun identifiant stocké (vérifié via `adb shell run-as fr.calaos.calaoshome ls/cat`).
- Mise à jour depuis une version avec identifiants dans `QSettings` : l'utilisateur reste connecté et la copie en clair a disparu.
- Le payload attendu côté relais est documenté (message `data`, champs).
