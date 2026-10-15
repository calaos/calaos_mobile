# T23 — Widget IO binaire générique (IOLight/IOOutlet/IOPump/IOBoiler/IOHeater)

| Champ | Valeur |
|---|---|
| Phase | P4 — Consolidation QML |
| Taille | M |
| Bloqué par | T07 (mêmes fichiers) |
| Groupe de conflit | G-QML-SHARED |
| Variantes | desktop + mobile |

## Contexte

Cinq widgets de `qml/SharedComponents/` partagent un squelette identique copy-pasté (`ItemBase { RowLayout { icône ; ScrollingText ; SensorStatusIcon ; 2× ItemButtonAction } }`) et ont dérivé indépendamment :

- `IOBoiler.qml` (59 l.) et `IOHeater.qml` (59 l.) sont **identiques au byte près** sauf le nom d'icône (ligne 19).
- **Icônes erronées** : `IOPump.qml:66,76`, `IOBoiler/IOHeater:41,51` utilisent `ic_outlet_on.svg`/`ic_outlet_off.svg` (icônes de *prise*) pour leurs boutons.
- `IOOutlet.qml:35-43` embarque une `RotationAnimation` 360° infinie qui n'a de sens que pour une pompe (collée depuis IOPump).
- **Ordre des boutons incohérent** : IOOutlet = on puis off (61,70), IOPump = off puis on (64,74).
- Dérive de casse sur la couleur du nom : `#3ab4d7` (IOLight:30, IOOutlet:47) vs Theme `#3AB4D7`.

## Fichiers

- **Nouveau** : `qml/SharedComponents/IOBinaryDevice.qml` → **à ajouter dans `qml_shared.qrc` ET `qml/SharedComponents/qmldir`**
- Réécrits en wrappers fins : `qml/SharedComponents/IOLight.qml`, `IOOutlet.qml`, `IOPump.qml`, `IOBoiler.qml`, `IOHeater.qml`
- `img_desktop.qrc` / `img_mobile.qrc` : **vérifier** l'existence des icônes pompe/chaudière/chauffage avant de les référencer

## Implémentation

1. `IOBinaryDevice.qml` : composant paramétré — `property string iconName`, `property url buttonOnIcon`, `property url buttonOffIcon`, options d'animation (rotation pour la pompe). Partir d'`IOLight.qml` comme **référence de comportement** (y compris le fix couleur de T07).
2. Les 5 widgets deviennent des wrappers de quelques lignes qui fixent les properties. **Conserver les 5 fichiers** pour ne pas toucher leurs consommateurs (`ItemListView.qml` les instancie par nom).
3. Corriger les icônes : si des icônes dédiées pompe/chaudière/chauffage existent dans `img/`, les utiliser ; sinon garder l'icône outlet mais le documenter en commentaire (ne pas créer d'assets dans ce ticket).
4. Homogénéiser l'ordre des boutons (off à gauche, on à droite — comme IOLight) et la couleur du nom via `Theme.blueColor`.
5. La `RotationAnimation` ne s'applique qu'à IOPump (via option du composant).

## Critères d'acceptation

- Les 5 types s'affichent et pilotent leur IO sur desktop **et** mobile.
- Screenshots avant/après joints à la PR pour chaque type (l'ordre des boutons et les icônes changent volontairement — le documenter).
- `qml/SharedComponents/qmldir` liste IOBinaryDevice + les 5 wrappers ; `qml_shared.qrc` à jour.
- qmllint OK.

## Vérification

Maison de démo ou serveur réel : actionner chaque type (lumière, prise, pompe, chaudière, chauffage) depuis la vue pièce sur les deux variantes.
