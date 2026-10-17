# IOS-2 — Push iOS sous Qt 6.12 : conflit avec le délégué d'application de Qt

| Champ | Valeur |
|---|---|
| Phase | P-DEV — Derniers appareils |
| Taille | M |
| Bloqué par | — |
| Groupe de conflit | G-IOS |
| Variantes | mobile (iOS) |

## Contexte

Constaté le 2026-10-17 sur l'iPhone 15 Pro (build `master` `c3beca3`, Qt 6.12.0) : l'app démarre et se connecte, mais aucun jeton APNs ne remonte (ni « Push device token », ni « Registering push token » dans la console `devicectl --console`), alors que FIX-3 journalise ces étapes.

Cause probable : `ios/HardwareUtils.mm` ajoute à `QIOSApplicationDelegate` une **catégorie** `(AppDelegate)` qui redéfinit `application:didFinishLaunchingWithOptions:`, `didRegisterForRemoteNotificationsWithDeviceToken:`, `didFailToRegisterForRemoteNotificationsWithError:`, `didReceiveRemoteNotification:fetchCompletionHandler:` et `performActionForShortcutItem:completionHandler:`. Qt 6.12 implémente désormais lui-même ces sélecteurs dans `libqios` (absents de Qt 6.11, sauf `didFinishLaunching`/`willFinishLaunching`). Deux implémentations du même sélecteur sur une classe (classe + catégorie, ou deux catégories) donnent un comportement indéfini côté Objective-C : la nôtre peut ne plus être appelée, ou casser celle de Qt.

Fonctions touchées potentiellement : enregistrement APNs et jeton (push), réception en arrière-plan, taps de notification (`UNUserNotificationCenterDelegate`), Quick Actions (raccourcis), réinitialisation du badge, style de barre d'état.

## Fichiers

- `ios/HardwareUtils.mm`, `ios/HardwareUtils_iOS.h`
- éventuellement `cmake/iOS.cmake`, `ios/AppInfo.plist`

## Implémentation

1. **Confirmer** sur l'iPhone (console `devicectl`, `log` éventuellement) quelles méthodes sont réellement appelées (ex. log temporaire), et lire ce que fait Qt 6.12 dans ces sélecteurs (sources Qt 6.12 `qtbase/src/plugins/platforms/ios/qiosapplicationdelegate.mm` si disponibles dans `~/Qt/6.12.0/Src` ou en ligne, notes de version) : Qt expose-t-il une API pour récupérer le jeton/les notifications (signal, `QNativeInterface`, notification `NSNotificationCenter`) ?
2. **Corriger sans catégorie conflictuelle** : utiliser le mécanisme prévu par Qt 6.12 s'il existe ; sinon un objet dédié (délégué `UNUserNotificationCenter` propre à l'app, observateurs `NSNotificationCenter` pour `UIApplicationDidFinishLaunchingNotification` / `DidBecomeActive`, et pour le jeton APNs soit l'API Qt, soit un swizzling ciblé et documenté qui appelle aussi l'implémentation de Qt). Quick Actions : même traitement.
3. Ne rien casser de FIX-3 / FIX-7 / FIX-11 (taps différés, `handleNotificationOpened`, jeton tronqué dans les logs, `setDeviceToken`).

## Critères d'acceptation

- Sur l'iPhone : au lancement, « Push device token <8 car>… » puis « Registering push token … for ios » après login (jeton jamais complet dans les logs).
- Un push Calaos est reçu app au premier plan, en arrière-plan et tuée ; le tap ouvre l'événement ; image de caméra via l'extension PushNotif.
- Une Quick Action (appui long sur l'icône) fonctionne.
- `tools/build_ios.sh Debug` et `--simulator` passent ; aucun avertissement de compilation nouveau.
