# MOB-1 — Zones de sécurité : Dynamic Island iOS et edge-to-edge Android 15+

| Champ | Valeur |
|---|---|
| Phase | P-DEV — Derniers appareils |
| Taille | M |
| Bloqué par | IOS-1 (captures de référence iOS), AND-4 |
| Groupe de conflit | G-QML-MOBILE |
| Variantes | mobile (iOS, Android) |

## Contexte

Aucune vue mobile ne gère les zones de sécurité : aucun usage de marges système dans `qml/mobile/` (vérifié le 2026-10-16).

- **iOS** : Dynamic Island / encoche en haut, barre d'accueil en bas (iPhone 14 Pro → 18, Air).
- **Android** : avec `targetSdk 36` (AND-2), Android 15+ **impose** l'affichage bord à bord (edge-to-edge) — l'opt-out `windowOptOutEdgeToEdgeEnforcement` n'est plus honoré en 36. Sur ces appareils, le contenu passe sous la barre d'état et sous la barre de navigation. Le téléphone de test (Android 12) ne le montre pas : vérification sur l'émulateur API 36 (`Calaos_API36_arm64`).

Qt 6.11 fournit l'attached property `SafeArea` (QtQuick, `SafeArea.margins`) qui expose les marges système sur iOS et Android.

## Fichiers

- `qml/mobile/main.qml` (racine, StackView, barre d'onglets), vues de `qml/mobile/` concernées
- `qml/SharedComponents/` si un composant d'en-tête/pied est partagé
- Couleur des barres système si nécessaire (`ios/AppInfo.plist` statut, côté Android via le thème ou Qt)

## Implémentation

1. Appliquer les marges `SafeArea` au niveau le plus haut possible (contenu racine / StackView / en-têtes et barre d'onglets), plutôt que vue par vue ; les fonds (couleur, images) peuvent continuer sous les barres système, le contenu interactif et le texte non.
2. Orientation paysage (iPad, tablettes Android) : marges gauche/droite également respectées.
3. Barre d'état lisible (texte clair sur fond sombre) sur iOS et Android 15+.
4. Pas de double marge sur les appareils sans encoche ni sur Android ≤ 14.

**Hors périmètre** : refonte visuelle des vues.

## Critères d'acceptation

Captures avant/après jointes au rapport, sur :
- simulateur iOS 27 iPhone 18 Pro Max et iPhone 17e (avec et sans Dynamic Island), iPad en paysage ;
- émulateur Android API 36 (`Calaos_API36_arm64`) ;
- téléphone Android 12 (non-régression : rien ne bouge).

Dans chaque cas : aucun texte ni bouton sous la barre d'état, l'encoche/Dynamic Island ou la barre de navigation/d'accueil ; écran de login, accueil, une pièce, liste des scénarios, réglages.
- `tools/build_ios.sh Debug` et `tools/build_android.sh Debug` passent ; `qmllint` sans nouvel avertissement sur les fichiers touchés.
