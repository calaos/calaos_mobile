# FIX-9 — App lancée hors ligne : se connecter quand le réseau revient

| Champ | Valeur |
|---|---|
| Phase | P-FIX — Petits bugs constatés pendant P-AND |
| Taille | S |
| Bloqué par | FIX-8 |
| Groupe de conflit | G-CONN |
| Variantes | mobile (Android surtout, iOS) |

## Contexte

Constaté pendant FIX-4, AND-7 et FIX-7 : au démarrage à froid sans réseau (ou avec une coupure brève au lancement), l'auto-login à 100 ms est refusé par le contrôle réseau de `Application::login()` (affiche « No network ») avant que la politique de reconnexion n'enregistre l'intention de session. Quand le réseau revient, `resume()` n'a rien à reprendre : l'app reste sur l'écran de login. Depuis FIX-7, iOS récupère dans le cas équivalent au retour au premier plan ; Android est le plus exposé.

## Fichiers

- `src/Application.cpp` (`login`, `networkStatusChanged`), `src/CalaosConnection.cpp|h` (`ReconnectPolicy`)

## Implémentation

Quand l'auto-login de démarrage (ou un login demandé) est refusé faute de réseau, enregistrer l'intention de session pour que le retour du réseau déclenche la connexion. Ne pas réactiver une session fermée volontairement (FIX-7). Garder une alerte « pas de réseau » non bloquante.

## Critères d'acceptation

- Tests unitaires (`tst_sessionlifecycle`) : démarrage hors ligne puis réseau ⇒ une connexion ; logout puis démarrage hors ligne ⇒ aucune.
- Sur appareil : lancer l'app en mode avion, couper le mode avion ⇒ connexion automatique.
