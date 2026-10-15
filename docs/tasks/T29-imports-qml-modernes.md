# T29 — Modernisation des imports QML (Qt5Compat, versions d'imports)

| Champ | Valeur |
|---|---|
| Phase | P5 — Polish |
| Taille | M |
| Bloqué par | T28 (fichiers potentiellement communs aux sweeps) |
| Groupe de conflit | G-QML-SWEEP |
| Variantes | desktop + mobile |

## Contexte

1. **Qt5Compat encore actif** : `Qt5Compat.GraphicalEffects` (`ColorOverlay`) est importé dans 6 fichiers — `qml/SharedComponents/ItemButtonAction.qml:2`, `qml/desktop/BasePopupView.qml:2`, `qml/desktop/DialogBookmarks.qml:4`, `qml/desktop/DialogTabList.qml:4`, `qml/desktop/Notification.qml:5`, `qml/mobile/CalaosTextField.qml:3`. Sous Qt6, le remplacement est `MultiEffect` (`QtQuick.Effects`).
2. **35 lignes d'imports versionnés Qt5** subsistent : `qml/quickflux/*` (`import QtQuick 2.5`), tous les `widgets/*` (`QtQuick 2.5`, `QtQuick.Layouts 1.3`), `qml/desktop/keyboard_style/style.qml:30-33` (`QtQuick 2.7`, `QtQuick.VirtualKeyboard 2.1`). Le reste du projet utilise les imports Qt6 non versionnés.
3. **Trois styles d'import quickflux** coexistent : `import "../quickflux"` (dominant), `import "qrc:/qml/quickflux"` (3 fichiers), et le module qmldir — à unifier sur un seul.

## Fichiers

- Les 6 fichiers Qt5Compat listés (re-grep `Qt5Compat` au démarrage du ticket — la liste peut avoir bougé)
- `qml/quickflux/*.qml`, `widgets/**/*.qml`, `qml/desktop/keyboard_style/style.qml`
- Les fichiers avec import quickflux non standard (grep `qrc:/qml/quickflux`)
- `desktop.pro:17` (`core5compat`) — **seulement si** plus aucun usage

## Implémentation

1. `ColorOverlay` → `MultiEffect { colorization: 1.0; colorizationColor: ... }`. ⚠️ MultiEffect a des différences de rendu (modèle de colorisation) : comparer visuellement chaque icône/élément migré, ajuster `colorization`/`brightness` si besoin.
2. Retirer les numéros de version des imports listés (style Qt6 : `import QtQuick`). Ne pas toucher aux fichiers sous `3rd_party/`.
3. Unifier les imports quickflux sur le style dominant `import "../quickflux"` (ou le module qmldir si trivial).
4. Si `grep -rn "Qt5Compat\|core5compat" qml/ widgets/ src/` = 0 après migration : retirer `core5compat` de `desktop.pro:17`. **Vérifier d'abord qu'aucun C++ ne l'utilise** (grep dans src/).

## Critères d'acceptation

- `grep -rn "Qt5Compat" qml/ widgets/` = 0.
- Rendu des icônes colorées inchangé (screenshots des 6 emplacements migrés).
- Plus d'imports versionnés Qt5 hors `3rd_party/` (grep `import QtQuick 2.`).
- Build desktop + mobile OK ; qmllint OK.

## Vérification

Lancement des deux variantes : boutons d'action d'IO, popups desktop, notifications, champ texte mobile, dialogs du navigateur — tous les endroits à ColorOverlay migré.
