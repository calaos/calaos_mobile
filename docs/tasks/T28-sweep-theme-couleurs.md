# T28 — Sweep couleurs : tokens Theme partout (sérialisé, dernier gros ticket QML)

| Champ | Valeur |
|---|---|
| Phase | P4 — Consolidation QML |
| Taille | L |
| Bloqué par | T27 |
| Groupe de conflit | G-QML-SWEEP — sérialisé (conflit potentiel avec tout fichier QML) |
| Variantes | desktop + mobile |

## Contexte

~328 couleurs hex codées en dur (desktop 183, SharedComponents 58, mobile 57, widgets 30) alors que le singleton `qml/SharedComponents/Theme.qml` définit déjà les valeurs dupliquées :
`blueColor "#3AB4D7"`, `yellowColor "#ffda5a"`, `redColor "#ff5555"`, `greenColor "#5fd35f"`, `whiteColor "#e7e7e7"`, `backgroundColor "#171717"`.

Theme n'est utilisé que dans 21 fichiers. Dérive de casse révélatrice : `#3ab4d7` vs `#3AB4D7` selon les copies. Même les fichiers récents du navigateur web (`SpeedDialPage`, `DialogTabList`, `DialogBookmarks`) recopient les valeurs de Theme en dur.

## Fichiers

- `qml/SharedComponents/Theme.qml` (enrichi d'abord)
- Sweep : `qml/desktop/`, `qml/mobile/`, `qml/SharedComponents/`, `widgets/`

## Implémentation

1. **Inventaire scripté préalable** (joint à la PR) : grep de tous les `#[0-9a-fA-F]{3,8}` dans qml/ et widgets/, clustering par valeur normalisée (casse-insensible), tri par fréquence.
2. Enrichir `Theme.qml` : pour chaque couleur fréquente (≥ ~3 occurrences) absente du thème, créer un token nommé sémantiquement (pas `color1` — ex. `dimTextColor`, `overlayColor`). Les couleurs quasi identiques à un token existant (dérive de casse ou d'une nuance non intentionnelle) sont **fusionnées** sur le token.
3. Sweep de remplacement. Critère d'exemption documenté : une couleur **orpheline** (1 occurrence, sémantique purement locale, ex. teinte d'un asset spécifique) peut rester locale.
4. S'assurer que chaque fichier touché importe `SharedComponents` (ou a accès au singleton Theme).

## Critères d'acceptation

- Nombre d'hex hors `Theme.qml` divisé par ~10 (chiffres avant/après dans la PR).
- Plus aucune occurrence des valeurs exactes des tokens Theme en dehors de Theme.qml (`grep -rni "#3ab4d7\|#ffda5a\|#ff5555\|#5fd35f\|#e7e7e7\|#171717" qml/ widgets/` = 0 hors Theme.qml).
- Diff visuel validé sur les vues principales des deux variantes (le rendu ne doit PAS changer — les fusions de dérives de casse sont invisibles).
- qmllint OK.

## Vérification

Campagne de screenshots avant/après (mêmes vues que T27) ; une différence de rendu = une fusion de couleur trop agressive à corriger.
