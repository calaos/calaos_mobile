# T24 — ItemListView : delegate map déclarative + qmldir complet

| Champ | Valeur |
|---|---|
| Phase | P4 — Consolidation QML |
| Taille | M |
| Bloqué par | T23 (qmldir + ItemListView en commun) |
| Groupe de conflit | G-QML-SHARED |
| Variantes | desktop + mobile |

## Contexte

1. `qml/SharedComponents/ItemListView.qml:64-98` : le choix du delegate par type d'IO est un **ternaire imbriqué de ~35 branches** (`model.ioType === Common.X ? compA : model.ioType === ...`). Illisible, source du bug `Switch_long` (corrigé en T07), et le dispatch est **dupliqué** : `IOSwitch.qml:24-60` re-dispatche sur `ioType` une seconde fois pour couleur/texte/blink.
2. `qml/SharedComponents/qmldir` omet **9 composants pourtant présents dans `qml_shared.qrc`** : `IOPump`, `IOOutlet`, `IOBoiler`, `IOHeater`, `IOAnalogStyled`, `IOSwitch`, `SensorStatusIcon`, `ScrollingText`, `SingleShotTimer`. Ils ne fonctionnent aujourd'hui que par instanciation locale via `import "."` — tout `import SharedComponents` externe échouerait à les résoudre.
3. Debug oublié : `ItemListView.qml:102` logge `console.debug("model is: "+…)` **à chaque chargement de delegate**.

## Fichiers

- `qml/SharedComponents/ItemListView.qml`
- `qml/SharedComponents/qmldir`
- `qml/SharedComponents/IOSwitch.qml` (simplification du re-dispatch si rendu inutile)

## Implémentation

1. Remplacer le ternaire par une structure déclarative — au choix :
   - une map JS `readonly property var delegateByType: ({ [Common.Light]: lightComp, [Common.SwitchLong]: switchComp, ... })` avec fallback `default_delegate` ;
   - ou `DelegateChooser` (`Qt.labs.qmlmodels`).
   Garantir la **couverture de chaque valeur** de `Common::IOType` (`src/Common.h`) : croiser l'enum avec la map, tout type non mappé tombe explicitement sur `default_delegate`.
2. Compléter `qmldir` avec les 9 composants manquants (singleton/version cohérents avec l'existant).
3. Retirer le `console.debug` de la ligne 102.
4. `IOSwitch.qml` : si la map permet de passer des paramètres au delegate, réduire le second dispatch interne ; sinon, au minimum le documenter (la vraie déduplication du concept type→présentation viendra avec l'exposition de la catégorie du registry T16 côté C++ — hors périmètre de ce ticket si trop invasif).

## Critères d'acceptation

- Tous les types d'IO d'une maison de démo s'affichent avec le bon delegate (desktop + mobile) — dresser la liste des types testés dans la PR.
- `qmldir` aligné avec le contenu de `qml_shared.qrc` (diff joint).
- qmllint OK ; plus de `console.debug` par delegate.

## Vérification

Parcours des pièces d'une maison de démo riche en types d'IO ; comparer visuellement avec l'état avant refactoring.
