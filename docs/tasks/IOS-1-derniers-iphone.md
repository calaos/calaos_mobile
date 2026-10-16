# IOS-1 — Support des derniers iPhone (iOS 27, iPhone 17/18, Air)

| Champ | Valeur |
|---|---|
| Phase | P-DEV — Derniers appareils |
| Taille | M |
| Bloqué par | — |
| Groupe de conflit | G-IOS |
| Variantes | mobile (iOS) |

## Contexte

Demande de Raoul le 2026-10-16 : vérifier et assurer le support des derniers iPhone. État constaté :

- Build : Xcode 27.1 (SDK iOS 27.1), Qt 6.11, cible iOS 17.0 (`cmake/iOS.cmake`, `tools/build_ios.sh`). Le build Debug passe et l'app tourne sur l'iPhone 15 Pro de Raoul (iPhone16,1, UDID 00008130-000955AE1EE3803A).
- `ios/AppInfo.plist` : pas de `CADisableMinimumFrameDurationOnPhone` (l'app reste à 60 Hz sur les écrans ProMotion 120 Hz) ; orientations iPhone portrait + « upside down » (non supporté par les iPhone Face ID) ; `NSPhotoLibraryUsageDescription` vague (« uses photos ») — vérifier si la permission est encore réellement utilisée.
- **Pas de manifeste de confidentialité** (`PrivacyInfo.xcprivacy`), exigé par l'App Store depuis 2024 pour les « required reason APIs » (l'app et l'extension PushNotif utilisent `NSUserDefaults`, dont une suite App Group ; Qt fournit déjà le sien pour ses frameworks).
- `ios/ressources.xcassets/AppIcon.appiconset` : ancien jeu multi-tailles (57, 72, 50… obsolètes). Xcode accepte désormais une icône unique 1024 px ; variantes sombre/teintée possibles.
- Zones de sécurité (Dynamic Island, barre d'accueil) : traitées dans le ticket partagé MOB-1, pas ici.

Simulateurs disponibles : iPhone 18 Pro / Pro Max, iPhone Air, iPhone 17 / 17 Pro / 17 Pro Max / 17e, iPhone 16e, iPad ; runtime iOS 27 en cours d'installation (`xcrun simctl list runtimes`).

## Fichiers

- `ios/AppInfo.plist`, `ios/PushNotif/Info.plist`
- **Nouveaux** : `ios/PrivacyInfo.xcprivacy` (app) et `ios/PushNotif/PrivacyInfo.xcprivacy` (extension)
- `cmake/iOS.cmake`, `ios/CMakeLists.txt` (embarquer les manifestes)
- `ios/ressources.xcassets/AppIcon.appiconset/`
- Éventuellement `tools/build_ios.sh` (cible simulateur)

## Implémentation

1. **Build simulateur** : permettre de builder et lancer l'app sur simulateur (option du script, ex. `tools/build_ios.sh Debug --simulator`, sans signature), si le kit Qt iOS contient les tranches simulateur arm64. Sinon, documenter la limite et faire les vérifications sur l'iPhone physique.
2. **Manifestes de confidentialité** pour l'app et l'extension : déclarer les required reason APIs réellement utilisées (au minimum `NSPrivacyAccessedAPICategoryUserDefaults` avec les bonnes raisons, dont celle de l'App Group) ; pas de tracking ; types de données collectées cohérents avec l'app (identifiants Calaos stockés localement, jeton push). Vérifier que les fichiers sont bien copiés dans `CalaosHome.app` et `PushNotif.appex`.
3. **120 Hz** : `CADisableMinimumFrameDurationOnPhone = true`.
4. **Orientations iPhone** : portrait uniquement (retirer upside-down) ; iPad inchangé.
5. **Icône** : passer à une icône unique 1024 px (à partir de `ItunesArtwork@2x.png`) ; variantes sombre/teintée seulement si un visuel adapté existe (sinon le noter en suivi).
6. `NSPhotoLibraryUsageDescription` : supprimer si inutilisée, sinon texte explicite.
7. **Matrice de vérification** sur simulateurs iOS 27 : iPhone 18 Pro Max, iPhone Air, iPhone 17e, iPad ; captures d'écran de l'écran de login et de l'accueil (mode démo `demo.calaos.fr`, ou login si accessible). Noter tout défaut d'affichage lié au notch/Dynamic Island pour MOB-1.

**Hors périmètre** : gestion des zones de sécurité dans le QML (MOB-1), Live Activities, widgets, App Intents.

## Critères d'acceptation

- `tools/build_ios.sh Debug` passe, l'app s'installe et tourne sur l'iPhone physique (`xcrun devicectl`).
- L'app tourne sur les simulateurs iOS 27 listés ci-dessus ; captures jointes au rapport.
- `PrivacyInfo.xcprivacy` présent dans le `.app` et dans le `.appex` (`find` sur le bundle) ; contenu justifié dans le rapport.
- `Info.plist` final du bundle contient `CADisableMinimumFrameDurationOnPhone`, orientations iPhone portrait seul.
- Le catalogue d'icônes compile sans avertissement (actool) et l'icône apparaît sur l'écran d'accueil du simulateur.
- Release : `tools/build_ios.sh Release --archive` produit une archive valide (sans upload).
