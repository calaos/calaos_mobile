# AND-8 — Le clavier reste ouvert après le login

| Champ | Valeur |
|---|---|
| Phase | P-AND — Android Qt 6 |
| Taille | S |
| Bloqué par | — |
| Groupe de conflit | G-QML-MOBILE |
| Variantes | mobile (Android, iOS) |

## Contexte

Constaté par Raoul le 2026-10-16 sur Redmi 9 (Android 12) avec le build AND-2 : après avoir tapé **Login**, l'app passe sur la page Home mais le clavier virtuel reste affiché ; il faut le fermer à la main.

`qml/mobile/LoginView.qml` contient trois champs texte (identifiant, mot de passe, hôte). `qml/mobile/main.qml` déclenche `calaosApp.login(...)` sur `onLoginClicked` puis pousse la vue Home dans le `StackView`. Rien ne retire le focus du champ actif ni ne ferme l'input method.

## Fichiers

- `qml/mobile/LoginView.qml` et/ou `qml/mobile/main.qml`

## Implémentation

- Au clic sur Login (et à la validation par la touche Entrée du clavier si elle déclenche le login) : retirer le focus des champs et fermer le clavier (`Qt.inputMethod.hide()`).
- Filet de sécurité : quand la vue de login quitte l'écran (passage à Home, y compris auto-login au démarrage), le clavier est fermé.
- Ne pas changer le comportement du login lui-même.

## Critères d'acceptation

- Sur le téléphone Android : après Login, la page Home s'affiche sans clavier.
- Échec de login (mauvais mot de passe) : on reste sur l'écran de login, comportement inchangé.
- Le QML passe `qmllint` sans nouvel avertissement sur les fichiers touchés.
- Pas de régression iOS (QML partagé ; vérification par build iOS suffisante).
