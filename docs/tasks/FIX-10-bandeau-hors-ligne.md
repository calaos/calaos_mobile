# FIX-10 — Bandeau « hors ligne » à la place de l'alerte modale « No network »

| Champ | Valeur |
|---|---|
| Phase | P-FIX — Petits bugs constatés pendant P-AND |
| Taille | S |
| Bloqué par | FIX-11, AND-9 |
| Groupe de conflit | G-QML-MOBILE + G-CONN |
| Variantes | mobile (Android, iOS) |

## Contexte

Décision de Raoul du 2026-10-16 (suivi de la revue FIX-9). Aujourd'hui, un login refusé faute de réseau affiche une alerte native modale « No network » (`Application.cpp`, `startLogin()`, via `HardwareUtils::showAlertMessage`). Depuis FIX-9, l'app se connecte d'elle-même quand le réseau revient, mais l'alerte reste affichée par-dessus l'accueil jusqu'à ce que l'utilisateur appuie sur Close : le message est périmé.

## Fichiers

- `src/Application.cpp|h` (exposer un état « hors ligne » au QML, retirer l'alerte modale des logins automatiques)
- `qml/mobile/main.qml` et/ou un nouveau composant `qml/mobile/OfflineBanner.qml` (+ `qml_mobile.qrc`)
- `lang/calaos_fr.ts`, `lang/calaos_de.ts` et les `.qm` versionnés (cf. suivi FIX-5 : régénérer les `.qm`)

## Implémentation

- Un bandeau non modal, discret et cohérent avec le thème sombre, en haut du contenu (sous l'en-tête, en respectant les zones de sécurité de MOB-1), visible tant que le réseau est absent et qu'une session est voulue ou qu'un login vient d'être refusé faute de réseau ; il disparaît dès le retour du réseau ou la connexion.
- Texte du type « Pas de réseau — connexion dès son retour » (FR/DE traduits).
- L'alerte modale disparaît pour les logins automatiques. Pour un appui sur Login hors ligne : bandeau aussi (pas d'alerte bloquante), avec éventuellement une courte mise en évidence.
- Pas de bandeau après un logout volontaire ni sur desktop.

## Critères d'acceptation

- Lancement en mode avion : bandeau visible, pas d'alerte modale ; retour du réseau ⇒ bandeau masqué et connexion (FIX-9).
- Coupure pendant une session active : bandeau pendant la coupure, masqué à la reconnexion.
- Captures iOS (simulateur) et Android (émulateur), clair dans le thème ; `qmllint` sans nouvel avertissement ; `.ts`/`.qm` FR/DE à jour.
