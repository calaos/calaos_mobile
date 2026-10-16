# AND-4 — Notifications push FCM via le SDK Firebase Java

| Champ | Valeur |
|---|---|
| Phase | P-AND — Android Qt 6 |
| Taille | L |
| Bloqué par | AND-3 |
| Groupe de conflit | G-ANDROID |
| Variantes | mobile (Android) |

## Contexte

Le push Android reposait sur le SDK Firebase C++ (retiré en AND-1). Décision du 2026-10-16 : passer au **SDK Firebase Java** standard (dépendance Gradle, plus de SDK externe). Le jeton FCM a le même format : le serveur et le relais `push.calaos.fr` ne changent pas.

Chemin actuel du jeton : `HardwareUtils::deviceToken` → `CalaosConnection` l'envoie au serveur avec `hw = "android"` (`src/CalaosConnection.cpp`, bloc `#if defined(Q_OS_ANDROID)`). Le serveur relaie les push par `https://push.calaos.fr/api/push` (calaos_base `NotifManager.cpp`).

Conventions : skill `qt6-android-jni` (le service FCM peut tourner **sans Qt chargé**).

## Fichiers

- `android/build.gradle` (plugin `com.google.gms.google-services`, Firebase BoM + `firebase-messaging`)
- `android/google-services.json` (existant, ne pas modifier)
- `android/AndroidManifest.xml` (service, permission `POST_NOTIFICATIONS`, canal par défaut)
- **Nouveau** : `android/src/fr/calaos/calaoshome/CalaosMessagingService.java`
- `android/src/fr/calaos/calaoshome/HardwareUtils.java`, `android/HardwareUtils_Android.cpp|h`, `cmake/Android.cmake`

## Implémentation

1. Gradle : Firebase BoM (version récente compatible AGP 9) + `firebase-messaging`, plugin google-services.
2. `CalaosMessagingService extends FirebaseMessagingService` : `onNewToken` persiste le jeton dans `SharedPreferences` et le transmet au C++ seulement si Qt est chargé ; `onMessageReceived` affiche une notification (titre/corps du payload) sur le canal Calaos, avec un `PendingIntent` qui ouvre l'activité et transporte `event_uuid` s'il est présent.
3. Activité : création du canal de notification ; demande de la permission `POST_NOTIFICATIONS` (Android 13+) au premier lancement ; au démarrage, lecture du jeton (`FirebaseMessaging.getInstance().getToken()` + valeur persistée) → `HardwareUtilsAndroid::setDeviceToken` (thread Qt).
4. Tap sur une notification : intent avec `event_uuid` → même chemin que iOS (`Application::pushNotificationReceived(uuid)`), app froide comme app déjà ouverte (`singleTop` / `onNewIntent`).
5. `CALAOS_ANDROID_PUSH` passe à `ON` par défaut, ou l'option est supprimée si elle n'a plus de raison d'être.
6. **Étape de vérification préalable** : documenter dans le rapport le format du payload envoyé par `push.calaos.fr` pour Android (message `notification` ou `data`) — impact direct sur AND-7. Si le relais utilise encore l'API FCM legacy (coupée en juin 2024), le signaler comme bloquant externe.

**Hors périmètre** : image de caméra dans la notification (AND-7).

## Critères d'acceptation

- Au lancement sur le téléphone : la permission de notification est demandée (Android 13+), un jeton FCM est obtenu et envoyé au serveur (log tronqué, jamais le jeton complet).
- Un push déclenché depuis le serveur Calaos est reçu app au premier plan, en arrière-plan et app tuée.
- Taper la notification ouvre l'app sur l'événement correspondant (event log).
- Aucune référence restante au SDK Firebase C++ dans le repo (hors historique git et doc legacy).
