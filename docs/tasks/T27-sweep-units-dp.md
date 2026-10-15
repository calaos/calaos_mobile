# T27 — Unification du sizing : Units.dp partout (sweep, sérialisé)

| Champ | Valeur |
|---|---|
| Phase | P4 — Consolidation QML |
| Taille | L |
| Bloqué par | T10, T23, T24, T25, T26 (tous les tickets QML ciblés mergés) |
| Groupe de conflit | G-QML-SWEEP — **sérialisé** : aucun autre ticket QML en parallèle |
| Variantes | desktop + mobile |

## Contexte

Deux systèmes de dimensionnement coexistent, parfois **dans le même fichier** :
- `Units.dp(x)` — le singleton (`qml/SharedComponents/Units.qml`) : ~753 usages ;
- `x * calaosApp.density` — multiplication directe : ~288 usages dans 38 fichiers.

Exemple : `IOLightDimmer.qml:9` (`height: 80 * calaosApp.density`) côtoie `:14` (`leftMargin: Units.dp(8)`). Les dialogs desktop `DialogNetInterface`, `DialogRGBColorPicker`, `DialogSensorDetails` utilisent `calaosApp.density` alors que le reste du desktop utilise `Units.dp`. Le bug de densité² de `main.qml` (corrigé en T07) est un symptôme direct de ce mélange.

## Fichiers

- Sweep sur `qml/desktop/`, `qml/mobile/`, `qml/SharedComponents/`, `widgets/`
- `qml/SharedComponents/Units.qml` (si un helper manque)

## Implémentation

1. **Avant tout** : lire `Units.qml` et vérifier l'équivalence exacte `Units.dp(x)` ≡ `x * calaosApp.density` (`Units.cachedValue` est bindé à `calaosApp.density` dans les main.qml). Si un écart existe (arrondi, cache), le documenter et le traiter.
2. Remplacement mécanique `N * calaosApp.density` → `Units.dp(N)` (et variantes `calaosApp.density * N`), **fichier par fichier**, avec vérification visuelle de la vue concernée à chaque groupe de fichiers.
3. Les usages non dimensionnels de `calaosApp.density` (s'il y en a — tests de seuil, etc.) sont laissés et listés dans la PR.
4. Ne PAS toucher à `Units.qml` lui-même ni aux `main.qml` au-delà du strict remplacement.

## Critères d'acceptation

- `grep -rn "calaosApp.density" qml/ widgets/` ≈ 0 (hors `Units.qml`/bindings d'initialisation dans les main.qml ; les exceptions restantes sont justifiées dans la PR).
- Parcours visuel complet des **deux variantes** (toutes les vues principales) sans régression de taille — sur au moins deux densités si possible (desktop density 1 + un device/simulateur mobile).
- qmllint OK.

## Vérification

Campagne visuelle : rooms, favoris, caméras, audio, réglages, dialogs desktop (NetInterface, RGBColorPicker, SensorDetails — les plus touchés), widgets.
