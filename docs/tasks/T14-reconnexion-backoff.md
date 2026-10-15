# T14 — Machine à états de reconnexion + backoff exponentiel ⚠️ ticket à risque

| Champ | Valeur |
|---|---|
| Phase | P2 — Robustesse réseau |
| Taille | L |
| Bloqué par | T13 |
| Groupe de conflit | G-CONN + G-APP |
| Variantes | desktop + mobile |

## Contexte

La logique de reconnexion est éclatée et défaillante :

1. **Tempête de reconnexion** : trois chemins indépendants planifient chacun un `QTimer::singleShot(1000, login)` sans coordination ni backoff — le handler `disconnected` (`Application.cpp:202-207`), `loginFailed` (`Application.cpp:506-511`), et `calaosServerDetected` (`Application.cpp:624-631`). Ils peuvent se chevaucher → plusieurs logins concurrents, boucle de retry serrée à 1 s.
2. **Double garde désynchronisable** : `Application::login` garde sur `applicationStatus` (`Application.cpp:380`), `CalaosConnection::login` garde séparément sur `constate` (`CalaosConnection.cpp:30`). Ces deux flags sont mis à jour à des endroits différents ; une erreur en vol peut laisser `applicationStatus==NotConnected` avec `constate!=ConStateUnknown` → le login suivant est un no-op silencieux.
3. **Un seul échec de poll → logout complet** : `startJsonPolling` connecte `errorOccurred → requestError` (`CalaosConnection.cpp:682`), et `requestError` (`458-464`) appelle `logout()` inconditionnellement. Une erreur transitoire vide toute l'UI.
4. **Double fallback WS→HTTP** : `onWsDisconnected` (209-218) **et** `onWsError` (221-233) appellent chacun `closeWebsocket()` puis `connectHttp()` ; un socket qui erre *et* se déconnecte déclenche les deux → deux logins HTTP concurrents.
5. **Clear avant garde** : `Application::loginFailed` (`Application.cpp:483-498`) vide les huit modèles (483-491) **avant** de vérifier `if (m_applicationStatus == Common::NotConnected) return;` (493) — un loginFailed parasite alors qu'on est déjà déconnecté re-vide tout.

## Fichiers

- `src/CalaosConnection.cpp/.h`
- `src/Application.cpp/.h`

## Implémentation

1. **Source de vérité unique** : CalaosConnection expose un enum d'état explicite (`Disconnected / Connecting / Connected / Reconnecting`) + signal de changement. `Application` s'y abonne et dérive `applicationStatus` de cet état au lieu de le gérer en parallèle. Le garde anti-double-login vit à UN seul endroit (CalaosConnection).
2. **Backoff exponentiel** : remplacer les trois `singleShot(1000, login)` par UN QTimer membre de reconnexion (dans Application ou CalaosConnection, au choix mais un seul), délai 1 s → ×2 → plafond 30 s, avec jitter (±20 %), remis à 1 s à la première connexion réussie. `calaosServerDetected` (découverte réseau) peut *court-circuiter* le délai en cours (déclencher un essai immédiat) mais pas empiler un timer de plus.
3. **Tolérance aux échecs de poll** : compteur d'échecs consécutifs dans le chemin de poll ; en dessous de 3 échecs → relancer le poll directement ; à 3 → logout + cycle de reconnexion. Réinitialiser le compteur à chaque poll réussi.
4. **Idempotence du fallback WS→HTTP** : flag « fallback déjà engagé » testé/posé dans `onWsDisconnected` et `onWsError`, remis à zéro par `login()`/`logout()`.
5. `loginFailed` : déplacer le garde « déjà déconnecté » **avant** le clear des modèles.

## Critères d'acceptation

- Logs : après coupure du serveur, les tentatives suivent 1/2/4/8/16/30/30… s **sans chevauchement** (un seul login en vol à la fois).
- Un unique échec de poll ne vide plus l'UI (les modèles restent peuplés, le poll repart).
- Retour du serveur → re-login automatique **unique**, backoff remis à zéro.
- Mauvais mot de passe → échec de login immédiat affiché, **pas** de retry infini agressif (le backoff s'applique, ou l'écran de login est réaffiché selon le flux existant — préserver le comportement actuel de l'écran de login sur credentials invalides).
- `make check` vert ; build desktop + mobile.

## Vérification (manuelle, obligatoire — desktop ET mobile)

1. Couper le serveur 2 min → observer la séquence de backoff dans les logs, un seul login à la reprise.
2. Mobile : mode avion on/off, verrouillage/déverrouillage (les handlers `applicationWillResignActive`/`applicationBecomeActive` de `Application.cpp:67-81` font logout/login — vérifier qu'un aller-retour rapide ne crée pas d'état zombie).
3. Desktop : suspend/resume machine.
4. Débrancher le câble pendant un long-poll → l'UI ne se vide pas au premier échec.
