# IOS-4 — Support de l'iPhone Duo (pliable) et captures App Store

| Champ | Valeur |
|---|---|
| Phase | P-DEV — Derniers appareils |
| Taille | M |
| Bloqué par | — |
| Groupe de conflit | G-IOS + G-QML-MOBILE |
| Variantes | mobile (iOS) |
| Échéance | avant avril 2027 (captures Duo obligatoires pour les apps construites avec le SDK iOS 27.1+) |

## Contexte

Constaté le 2026-10-22 en préparant la release 4.1.0 : App Store Connect propose des captures pour l'**iPhone Duo** (écran extérieur 1398 × 2034, écran intérieur 2007 × 2853, portrait). Elles sont facultatives aujourd'hui et deviendront obligatoires à partir d'avril 2027 pour toute app construite avec le SDK iOS 27.1 ou plus récent ; les fournir affiche le badge « optimisée pour iPhone Duo » sur la fiche App Store. Décision de Raoul : pas de captures Duo pour la 4.1.0.

Premier essai sur le simulateur « iPhone Duo » (iOS 27.1, Xcode 27.1, Qt 6.12.0, build `release/ios-4.1.0`) :
- l'écran intérieur (`simctl io screenshot`, 2007 × 2853) reste entièrement noir ;
- dès le lancement, l'app journalise « Application is in background, drop the session » puis « Launch: no automatic login » ; elle ne se connecte pas ;
- `simctl io enumerate` liste plusieurs écrans (intérieur 2007 × 2853, un écran 7680 × 4320, d'autres) ; la capture de l'écran extérieur via `simctl io … screenshot --display=…` s'est bloquée.

Hypothèses : le simulateur démarre plié et la scène de l'app est sur l'écran extérieur (ou nulle part), et/ou Qt 6.12 ne gère pas le passage d'une scène UIKit entre écrans d'un appareil pliable ; le cycle de vie (resign/become active à la pliure) déclenche la mise en pause FIX-7.

## Fichiers (à confirmer)

- `ios/HardwareUtils.mm` (scène, cycle de vie), `ios/AppInfo.plist` (orientations, scènes)
- `qml/mobile/main.qml` (mise en page selon la taille de fenêtre, MOB-1 safe areas)
- `tools/build_ios.sh` / skill `qt6-ios-build` (procédure simulateur Duo)

## Implémentation

1. Comprendre le modèle du simulateur Duo (comment plier/déplier, quel écran est actif, comment capturer chaque écran) et ce que fait Qt 6.12 (notes de version Qt, bugs connus, `QIOSWindowSceneDelegate`, changements d'écran de scène).
2. Faire fonctionner l'app plié et déplié : rendu sur l'écran actif, pas de fausse mise en arrière-plan à la pliure (ou reprise correcte, FIX-7), session conservée, mise en page adaptée à l'écran intérieur (presque carré ; orientations iPhone portrait seul aujourd'hui — vérifier ce qu'Apple attend pour le Duo).
3. Produire les captures App Store Duo (écran extérieur et intérieur) à partir d'une vraie maison ou du mode démo, et documenter la procédure.

## Critères d'acceptation

- Sur simulateur iPhone Duo : l'app s'affiche et fonctionne plié et déplié, et au passage de l'un à l'autre (connexion conservée, pas d'écran noir).
- Captures 1398 × 2034 et 2007 × 2853 produites et jointes au rapport.
- Pas de régression sur les autres iPhone (simulateur iPhone 18 Pro Max, iPhone 17e) ni sur iPad.
