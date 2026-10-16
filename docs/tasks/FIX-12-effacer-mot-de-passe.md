# FIX-12 — Le bouton « effacer » du champ mot de passe n'efface rien

| Champ | Valeur |
|---|---|
| Phase | P-FIX — Petits bugs constatés pendant P-AND |
| Taille | S |
| Bloqué par | FIX-10 |
| Groupe de conflit | G-QML-MOBILE |
| Variantes | mobile (Android, iOS) |

## Contexte

Constaté pendant la revue FIX-10 (émulateur Android 16, 2026-10-16) : sur l'écran de login, le bouton d'effacement (X) du champ mot de passe (`qml/mobile/LoginView.qml` ~122, via `qml/mobile/CalaosTextField.qml`) ne vide pas le champ. Le bouton appelle `cut()`, sans effet en mode `TextInput.Password` (le presse-papiers refuse les mots de passe).

## Fichiers

- `qml/mobile/CalaosTextField.qml` (et `qml/mobile/LoginView.qml` si nécessaire)

## Implémentation

Vider le champ explicitement (`text = ""` ou `clear()`), pour tous les modes d'écho, sans casser le comportement des autres champs ni la synchronisation `LoginView.syncFromApp()` (FIX-8).

## Critères d'acceptation

- Le bouton X vide le champ mot de passe, l'identifiant et l'hôte.
- `qmllint` sans nouvel avertissement ; vérification sur émulateur Android et simulateur iOS.
