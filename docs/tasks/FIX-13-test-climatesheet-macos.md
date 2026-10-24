# FIX-13 — `ClimateSheet::test_theModelLetsGoOfTheDeviceWhenTheSheetCloses` échoue en local sur macOS

| Champ | Valeur |
|---|---|
| Phase | P-FIX — Petits bugs constatés pendant P-AND |
| Taille | S |
| Bloqué par | — |
| Groupe de conflit | G-QML-MOBILE |
| Variantes | tests (`tst_climatewidget`) ; mobile si la cause est dans `ClimateSheet.qml` |

## Contexte

Constaté pendant AP2b (2026-10-24, macOS, Qt 6.12.0, `QT_QPA_PLATFORM=offscreen`) : le cas `test_theModelLetsGoOfTheDeviceWhenTheSheetCloses` de `tests/tst_climatewidget/tst_climatesheet.qml` échoue à chaque exécution du fichier complet (`Actual: IOBase(0x…)`, `Expected: null`, ligne `compare(host.climate.modelData, null)`), y compris sans aucune modification par rapport à `master`. Il passe sur la CI Linux de `master`, et il passe en local lorsqu'il est lancé seul (`./tst_climatewidget -input …/tst_climatesheet.qml ClimateSheet::test_theModelLetsGoOfTheDeviceWhenTheSheetCloses`). L'échec dépend donc de l'ordre ou de l'état laissé par les cas précédents du même fichier, pas seulement de la machine.

## Fichiers

- `tests/tst_climatewidget/tst_climatesheet.qml` (le cas, `open()`, `init()`/`cleanup()`)
- `qml/mobile/ClimateSheet.qml` (`onAboutToHide` → `host.wanted = false`, `onClosed` → `climate.modelData = null`)
- `qml/mobile/BottomSheet.qml` (transitions d'ouverture/fermeture)

## Implémentation

Investiguer, sans corriger dans un premier temps :
- l'attente fixe `wait(300)` après `!host.opened` : `closed()` peut-il arriver plus tard (transition de sortie, animation) que `opened` ne retombe ?
- un état qui fuit d'un cas à l'autre : `host.wanted` resté à `true`, un hôte précédent encore vivant (`hosts`, `createTemporaryObject`), une sheet encore dans `Overlay.overlay` ;
- une différence de plateforme (ramasse-miettes JS, ordre de destruction, rendu offscreen macOS).

Si la cause est dans le test, remplacer l'attente fixe par une condition (`tryVerify`/`tryCompare` sur `modelData`) ou isoler l'état ; si elle est dans `ClimateSheet.qml`, corriger le relâchement du modèle.

## Critères d'acceptation

- Le fichier `tst_climatesheet.qml` complet passe en local sur macOS (au moins 5 exécutions consécutives) et sur la CI Linux.
- La cause est expliquée dans le commit.
