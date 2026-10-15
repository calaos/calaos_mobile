# T25 — PageScaffold desktop : factoriser le squelette header/footer

| Champ | Valeur |
|---|---|
| Phase | P4 — Consolidation QML |
| Taille | M |
| Bloqué par | T07 (main.qml desktop mergé) |
| Groupe de conflit | G-QML-DESKTOP (fichiers disjoints de T26 → parallélisable avec T26) |
| Variantes | desktop uniquement |

## Contexte

Le squelette de page desktop — ombres `header_shadow`/`footer_shadow`, `BorderImage` `standard_list_decoration`, bandeaux `module_header`/`module_footer` + RowLayout — est **copy-pasté dans la plupart des vues plein écran** :
- `qml/desktop/ScenarioView.qml:8-98` est essentiellement ce boilerplate repris de `RoomDetailView.qml:10-72,185-247` ;
- `qml/desktop/CameraListView.qml:7-34,153-210` — même scaffold.

Chaque évolution du chrome desktop doit aujourd'hui être répliquée à la main dans N vues.

## Fichiers

- **Nouveau** : `qml/desktop/PageScaffold.qml` → **à ajouter dans `qml_desktop.qrc`**
- Migrés dans ce ticket : `qml/desktop/ScenarioView.qml`, `qml/desktop/RoomDetailView.qml`, `qml/desktop/CameraListView.qml`
- Les autres vues utilisant le pattern (grep `standard_list_decoration` et `module_header` dans qml/desktop/) sont **listées dans la PR** mais migrées ultérieurement (limiter le rayon du ticket)

## Implémentation

1. `PageScaffold.qml` : composant avec `default property alias content`, slots/properties pour le contenu du header et du footer (titres, boutons), reproduisant **exactement** le rendu actuel (mêmes images, mêmes marges).
2. Migrer les 3 vues cibles : leur contenu spécifique passe dans le scaffold, le boilerplate disparaît.
3. Documenter l'usage en tête de `PageScaffold.qml` (3-4 lignes d'exemple).

## Critères d'acceptation

- Rendu **pixel-perfect identique** sur les 3 vues migrées : screenshots avant/après joints à la PR (mêmes dimensions de fenêtre).
- Liste des vues restantes à migrer incluse dans la description de PR.
- qmllint OK ; build desktop OK.

## Vérification

Lancement desktop, navigation Scénarios / détail de pièce / liste caméras, comparaison visuelle attentive (ombres, marges, bandeaux).
