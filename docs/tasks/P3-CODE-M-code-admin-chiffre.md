# P3-CODE-M — Code admin chiffré : l'app affiche le code, la notification ne le contient plus

| Champ | Valeur |
|---|---|
| Phase | P-ADMIN — Administration de la box, phase 3 (amendement (a) du `docs/admin/PLAN.md` de calaos_base) |
| Taille | L |
| Bloqué par | AP2b ; côté serveur P3-CODE-S (calaos_base `admin/p8`) |
| Groupe de conflit | G-CONN + G-APP + G-QML-MOBILE + G-ANDROID + G-IOS |
| Variantes | mobile (iOS, Android) ; desktop : rien (pas de clé, pas de `push_key`) |

## Contexte

Le code 2FA de 6 chiffres envoyé au téléphone approbateur était dans le texte de la notification : écran verrouillé, relais push, FCM et APNs le voyaient. Le contrat de calaos_base (`docs/08_http_api.md`, « Code push chiffré », branche `admin/p8`) fait sceller le code par calaos_server avec une **clé push** propre au téléphone ; la notification ne porte qu'un avis générique, l'app déchiffre et affiche le code.

## Fichiers

- `src/AdminCode.h|cpp` (construction, vérification, échéance, `AdminCodeModel`) ; `calaos.pri`, `cmake/CalaosHomeSources.cmake`
- `src/HardwareUtils.h|cpp` (clé push, réserve, `adminCodeNotified`, présentation au premier plan)
- `src/CalaosConnection.h|cpp`, `src/JsonKeys.h` (`push_key`, `PushRegistration`, masquage)
- `src/Application.h|cpp` (modèle, prêt une fois l'UI chargée, « Reset all data »)
- `qml/mobile/AdminCodeSheet.qml`, `qml/mobile/main.qml`, `qml_mobile.qrc`
- `ios/HardwareUtils.mm`, `ios/HardwareUtils_iOS.h`, `ios/PushNotif/NotificationService.m`
- `android/src/fr/calaos/calaoshome/{CalaosMessagingService,CredentialStore,HardwareUtils}.java`, `android/HardwareUtils_Android.{h,cpp}`, `android/debug/.../FakePushReceiver.java`
- `lang/calaos_fr.ts|qm`, `lang/calaos_de.ts|qm`
- `tests/tst_admincode/` (nouveau), `tests/tst_pushnotif/`, `tests/tst_mobilesettings/`
- `doc/mobile.md`, `doc/android.md`, `doc/ios.md`, `tests/README.md`

## Implémentation

- **Clé push** : 32 octets du CSPRNG de la plateforme, créés au premier lancement. iOS : élément de trousseau `calaos-push-key` (`SecRandomCopyBytes`, `AfterFirstUnlockThisDeviceOnly`, service `CalaosMobile.push`, même groupe d'accès que les identifiants : l'extension pourrait la lire, elle ne déchiffre rien). Une clé n'est recréée que si le magasin dit qu'elle n'existe plus ; une erreur passagère (trousseau verrouillé, Keystore) laisse la clé vide et la lecture est retentée au retour au premier plan. Android : `CredentialStore.pushKey()` (`SecureRandom`, chiffrée par une clé Keystore dédiée `calaos_push_key_v1` dans `calaos_push.xml`, exclu de la sauvegarde). Jamais dans QSettings, jamais journalisée. Conservée par « Reset all data » (la clé d'un approbateur est gelée sur sa box).
- **`register_push`** : `push_key` en 64 hexadécimaux minuscules (racine en HTTP, `data` en WebSocket), aussi hors TLS (décision du 30/10), absent sans clé ; `PushRegistration` renvoie quand le jeton, le nom **ou la clé** change ; `push_key` masqué dans le dump debug HTTP.
- **Réception, sans cryptographie** : la charge (`admin_code_id`, `_action`, `_expires`, `_nonce`, `_ct`, `_tag`) est stockée telle que reçue avec `received_at` (horloge murale, ms) et `received_mono` (horloge de boot : `elapsedRealtime()` Android, `CLOCK_MONOTONIC` Darwin, qui avancent pendant la veille et sont communes aux processus). Android : `CalaosMessagingService` → `CredentialStore` (`admin_codes`) puis notification générique (`calaos_events`, id `admin_code_id.hashCode()`, extra `admin_code_id`) puis `emitAdminCodeNotified` si Qt tourne. iOS : l'extension écrit `admin_codes/<code_id>.json` dans le conteneur App Group ; l'app stocke elle-même ce que l'extension n'a pas vu (présentée, tapée, ou encore dans le centre de notifications au retour au premier plan). Une notification balayée ne perd rien.
- **Déchiffrement et affichage** (`AdminCodeModel`) : à la fin du chargement de l'UI, à chaque `adminCodeNotified` et à chaque retour au premier plan. Formes vérifiées avant le tag (action de la table, `expires` strict, hexadécimal minuscule de bonne longueur), tag recalculé sur les chaînes reçues et comparé en temps constant, puis `code = ct XOR ks[0..6)`. Tag faux : abandonné, journalisé sans valeur, et la feuille indique « Un code est arrivé mais n'a pas pu être lu sur ce téléphone ». Feuille `AdminCodeSheet` : objet, 6 chiffres, compte à rebours à la seconde avec barre, Copier (iOS : élément du presse-papiers qui expire à 60 s ; ailleurs vidé après 60 s s'il contient encore le code), Fermer ; plusieurs codes, le plus récent d'abord ; « Expiré » à l'échéance.
- **Échéance** : 60 s à partir de la réception sur le téléphone, jamais `expires` (horloge de la box) comparé à l'horloge du téléphone. Le plus grand des âges mural / boot compte ; horloge de boot sous le tampon (redémarrage) ou horloge murale reculée : abandon. Expirés et rejetés quittent la réserve aussitôt, les codes affichés à la fermeture de la feuille (notification retirée aussi).
- **Premier plan iOS** : pas de bannière pour une charge chiffrée, la feuille s'ouvre.
- **Texte hérité** : téléphone sans clé ou box ancienne → le texte en clair arrive comme avant, rien ne change sur ce chemin.

## Hors périmètre

- Côté serveur et webapp (P3-CODE-S, déjà fusionné sur `admin/p8`).

## Critères d'acceptation

- `tst_admincode` : les 3 vecteurs de référence passent exactement ; chaque cas « à refuser » du contrat est refusé ; échéance comptée depuis la réception ; réserve : plus récent d'abord, expirés et forgés retirés.
- `tst_pushnotif` : `push_key` dans `register_push` en HTTP et en WebSocket, renvoi au changement de clé, absent sans clé (chemin hérité inchangé), jamais en clair dans les logs.
- `tst_mobilesettings` : feuille du code (compte à rebours, « Expiré », Copier, plusieurs codes).
- `make check` vert (hors FIX-13) ; build desktop ; `tools/build_ios.sh Debug` (avec PushNotif) ; `tools/build_android.sh Debug` ; `tools/check_sources.sh`.
- Sur appareil (orchestrateur) : code reçu d'une box `admin/p8`, notification sans code, feuille ouverte au tap et au premier plan ; Android : vecteur 1 rejoué par `FakePushReceiver` affiche `482913`.
