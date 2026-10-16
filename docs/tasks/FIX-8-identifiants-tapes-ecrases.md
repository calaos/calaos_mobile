# FIX-8 — Un changement de réseau écrase les identifiants tapés sur l'écran de login

| Champ | Valeur |
|---|---|
| Phase | P-FIX — Petits bugs constatés pendant P-AND |
| Taille | S |
| Bloqué par | FIX-7 |
| Groupe de conflit | G-CONN |
| Variantes | mobile (Android, iOS), desktop à vérifier |

## Contexte

Constaté pendant FIX-7 (émulateur, 2026-10-16) et confirmé par sa revue : `Application::networkStatusChanged()` (et `calaosServerDetected`) appelle `loadSettings()` avant `resume()`, même quand `resume()` ne fait rien. Après un login manuel raté, un simple changement de réseau remplace dans les champs de l'écran de login l'identifiant, le mot de passe et l'hôte tapés par ceux du trousseau. Observé : un mauvais mot de passe tapé a été remplacé par les identifiants démo enregistrés et l'app est passée en mode démo. Lié : l'écran de login affiche parfois d'anciennes valeurs après un logout (liaison QML cassée par la saisie).

## Fichiers

- `src/Application.cpp` (`networkStatusChanged`, `calaosServerDetected`, `loadSettings`)
- `qml/mobile/LoginView.qml` (liaisons des champs)

## Implémentation

Ne recharger les réglages que lorsqu'une reprise de session va réellement avoir lieu (`canResume()`), et faire en sorte que la reprise utilise les identifiants de la dernière session acceptée, sans toucher aux champs en cours de saisie. Vérifier que l'écran de login affiche des valeurs cohérentes après un logout.

## Critères d'acceptation

- Test unitaire ou de scénario : identifiants tapés non acceptés + changement de réseau ⇒ champs inchangés, aucune connexion avec d'autres identifiants.
- Session active + changement de réseau ⇒ reprise avec les identifiants de la session.
- Sur appareil : taper un mauvais mot de passe, couper/rétablir le réseau ⇒ les champs ne changent pas.
