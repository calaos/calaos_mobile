# FIX-14 — Push invisibles quand l'app est au premier plan (code admin)

| Champ | Valeur |
|---|---|
| Phase | P-FIX — Petits bugs constatés pendant P-AND |
| Taille | S |
| Bloqué par | — |
| Groupe de conflit | G-IOS + G-ANDROID |
| Variantes | mobile (iOS ; Android vérifié) |

## Contexte

Constaté le 2026-10-24 : app fermée ou en arrière-plan, les push s'affichent ; app au premier plan, rien ne s'affiche, en particulier le code admin (« Calaos admin code NNNNNN: … ») envoyé quand l'admin ouvre la console web.

Payload (calaos_base `NotifManager::buildPushBody()`, appelé par `sendPushNotificationTo()` depuis `AdminEvents::processPushCode()`) : le code admin est un push ordinaire **sans `event_uuid`**. iOS : alerte `message` + `mutable_content`, pas de `data`. Android, jeton `push_format` « data » : message data-only `{ title, body, message }` ; jeton ancien format : `notification` + `data`.

iOS : `CalaosNotificationDelegate userNotificationCenter:willPresentNotification:` (`ios/HardwareUtils.mm`) répondait toujours `UNNotificationPresentationOptionNone`. Un push d'événement ouvre la visionneuse dans l'app (`pushNotifReceived` → `Application::pushNotificationReceived()`), mais seulement si une session est ouverte ; un push sans `event_uuid` n'était montré nulle part.

Android : `CalaosMessagingService.onMessageReceived()` → `handleMessage()` poste la notification dans tous les états, sans condition de premier plan ni de session, pour les deux formats (en ancien format, `onMessageReceived()` n'est appelé qu'au premier plan, justement). Pas de bug dans le code.

## Fichiers

- `src/HardwareUtils.h` (`foregroundPushPresentation()`, header-only)
- `ios/HardwareUtils.mm` (`willPresentNotification:`)
- `tests/tst_pushnotif/tst_pushnotif.cpp`
- `doc/ios.md` (section 4)

## Implémentation

- iOS : présenter le push au premier plan avec bannière + liste + son. Un push d'événement avec une session ouverte : liste + son seulement (la visionneuse est déjà à l'écran, une bannière par-dessus ferait doublon). Jamais l'option badge (le compteur est remis à zéro, l'utilisateur regarde l'app). L'émission vers l'app est conservée. Cible iOS 18 : pas de repli `Alert` pour iOS < 14.
- Android : aucun changement.
- Ni format de push ni serveur modifiés, aucune dépendance.

## Critères d'acceptation

- `tst_pushnotif` : les quatre cas (code admin / événement × connecté / déconnecté) de `foregroundPushPresentation()` passent.
- Builds Debug iOS (appareil) et Android verts, installés sur l'iPhone et le téléphone Android.
- Sur appareil, app au premier plan et connectée : le code admin s'affiche en bannière (iOS) / notification heads-up (Android) avec son ; un push d'événement ouvre la visionneuse et reste dans le centre de notifications (iOS).
