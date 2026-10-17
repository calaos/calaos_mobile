# IOS-3 — Pastille de l'icône iOS : compteur de notifications non lues

| Champ | Valeur |
|---|---|
| Phase | P-DEV — Derniers appareils |
| Taille | S |
| Bloqué par | IOS-2 |
| Groupe de conflit | G-IOS |
| Variantes | mobile (iOS) |

## Contexte

Constaté par Raoul le 2026-10-17 pendant le passage iPhone : aucune pastille (badge) sur l'icône à la réception d'un push. Le payload iOS de `calaos_server` ne contient pas de `badge` ; l'app ne fait que remettre la pastille à zéro à l'ouverture. Ce n'est pas une régression.

Décision de Raoul (option 2) : l'extension `PushNotif` compte les notifications non lues dans un compteur partagé via l'App Group (`group.fr.calaos.CalaosMobile`) et fixe `bestAttemptContent.badge`. Côté serveur, `calaos_server` pose `mutable-content` sur toutes les notifications iOS pour que l'extension s'exécute aussi sans image (changement confié à la session calaos_base).

## Fichiers

- `ios/PushNotif/NotificationService.m`
- `ios/HardwareUtils.mm` (remise à zéro du compteur et de la pastille)
- `ios/PrivacyInfo.xcprivacy` / `ios/PushNotif/PrivacyInfo.xcprivacy` (commentaires si l'usage de l'App Group change)
- `doc/ios.md`

## Implémentation

- Extension : à chaque push, incrémenter atomiquement un compteur `calaos.badge_count` dans les `NSUserDefaults` de l'App Group et fixer `bestAttemptContent.badge` à sa valeur, sur tous les chemins (avec ou sans image, y compris `serviceExtensionTimeWillExpire`). Le téléchargement d'image (et la règle FIX-11 « pas d'image après logout ») reste inchangé.
- App : remettre le compteur et la pastille à 0 au lancement, au retour au premier plan, et quand un push est reçu app au premier plan (l'utilisateur le voit déjà) ; après « Réinitialiser toutes les données », effacer la clé.
- Push sans `mutable-content` (serveur pas encore à jour) : l'extension ne tourne pas, aucune pastille ; pas de régression.

## Critères d'acceptation

- Sur l'iPhone (serveur à jour) : app en arrière-plan ou tuée, 3 push ⇒ pastille 3 ; ouvrir l'app ⇒ pastille effacée ; push app au premier plan ⇒ pas de pastille résiduelle.
- Push avec image : image toujours présente et pastille incrémentée.
- `tools/build_ios.sh Debug` et `--simulator` passent sans nouvel avertissement.
