# T03 — CI de PR : build + tests + qmllint

| Champ | Valeur |
|---|---|
| Phase | P0 — Fondations |
| Taille | M |
| Bloqué par | T01, T02 |
| Groupe de conflit | G-BUILD |
| Variantes | infrastructure |

## Contexte

Les workflows existants (`.github/workflows/`) ne compilent **jamais** le code : `build.yml` fait un simple `repository-dispatch` vers `calaos/calaos-build`, les autres construisent des images Docker. Une PR qui casse la compilation merge en vert. Aucun lint QML n'est exécuté.

## Fichiers

- **Nouveau** : `.github/workflows/pr-ci.yml`
- **Ne pas modifier** les workflows existants (`build.yml`, `build_dev.yml`, `build_release.yml`, `build_docker_*.yml`).

## Implémentation

Workflow déclenché sur `pull_request` et `push` vers `master`, runner `ubuntu-latest`, trois jobs :

1. **build-desktop** : installer Qt6 via apt (`qt6-base-dev qt6-declarative-dev qt6-websockets-dev qt6-5compat-dev qt6-svg-dev libx11-dev libxext-dev` + `qmake6`) puis `qmake6 desktop.pro && make -j$(nproc)`. Le module `webenginequick` est optionnel grâce au garde `qtHaveModule` de `desktop.pro:19-22` — ne pas l'installer.
2. **tests** : `qmake6 tests/tests.pro && make -j && make check` (dépend du harnais T02).
3. **qmllint** (mode pragmatique) : exécuter `qmllint` (paquet `qt6-declarative-dev-tools`) sur `qml/**/*.qml` et `widgets/**/*.qml` avec sortie `--json`. Le code legacy produit des centaines de warnings (« unqualified access ») : **n'échouer que sur les erreurs** (syntaxe, référence introuvable), publier le rapport complet en artefact. Le durcissement viendra après la phase P4.

Épingler les actions utilisées par version majeure récente (`actions/checkout@v4`…).

## Critères d'acceptation

- Le workflow est vert sur `master` une fois T01 + T02 mergés.
- Une PR introduisant une erreur de compilation C++ est bloquée (statut rouge).
- Une PR introduisant une erreur de syntaxe QML est bloquée.
- Le rapport qmllint complet est téléchargeable en artefact de run.

## Vérification

Ouvrir une PR de test avec une erreur volontaire (puis la fermer) ou pousser sur une branche et vérifier le run. Durée totale du workflow < 15 min souhaitée.
