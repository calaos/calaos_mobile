# FIX-7 — Pas de reconnexion automatique après une déconnexion volontaire

| Champ | Valeur |
|---|---|
| Phase | P-FIX — Petits bugs constatés pendant P-AND |
| Taille | S |
| Bloqué par | AND-7 |
| Groupe de conflit | G-CONN |
| Variantes | mobile (Android, iOS), desktop à vérifier |

## Contexte

Constaté pendant AND-7 (2026-10-16, émulateur Android 16) : après un « Se déconnecter » explicite (Réglages → Logout, `Application::userLogout()`), une coupure réseau suivie d'un retour reconnecte l'app toute seule. Sept secondes après le logout, l'app était revenue sur l'accueil (`get_home`), et `homeLoaded()` avait remis `session_active` à vrai, ce qui réactive aussi le téléchargement des images de caméra dans les notifications (AND-7).

Chemin : `Application::networkStatusChanged()` → `calaosConnect->suspend()` sur `Disconnected`, puis `resume()` sur `Online`. `ReconnectPolicy::suspend()` rend `canResume()` vrai même après que `logout()` a appelé `reconnectPolicy.stop()`. Depuis FIX-4, Android émet ces changements de réseau, donc un simple changement Wi-Fi/4G suffit sur un téléphone ; iOS (Reachability) a le même défaut latent.

## Fichiers

- `src/CalaosConnection.cpp|h` (`ReconnectPolicy`, `suspend`, `resume`, `logout`)
- `src/Application.cpp` (`networkStatusChanged`, `logout`, `userLogout`)
- `tests/` (harnais Qt Test, ex. `tst_pushnotif` ou un nouveau test de la politique)

## Implémentation

`suspend()` ne doit pas réarmer une politique arrêtée par un logout : une connexion volontairement fermée reste fermée jusqu'au prochain login explicite (ou auto-login au démarrage). Ne pas casser la reprise normale (réseau perdu puis retrouvé pendant une session active, retour au premier plan sur iOS).

## Critères d'acceptation

- Test unitaire : logout → suspend → resume ⇒ aucune tentative de connexion ; session active → suspend → resume ⇒ reconnexion (comportement actuel conservé).
- Sur appareil (passage groupé) : Logout, puis mode avion on/off ⇒ l'app reste sur l'écran de login.
- Builds Android, iOS, desktop OK.
