# FIX-11 — Mémoriser la déconnexion volontaire entre deux lancements

| Champ | Valeur |
|---|---|
| Phase | P-FIX — Petits bugs constatés pendant P-AND |
| Taille | S |
| Bloqué par | FIX-9 |
| Groupe de conflit | G-CONN + G-ANDROID |
| Variantes | mobile (Android, iOS) |

## Contexte

Décision de Raoul du 2026-10-16 (suivi de la revue FIX-9). Après « Se déconnecter » (`Application::userLogout()`), fermer puis relancer l'app reconnecte automatiquement : l'auto-login de lancement (100 ms) utilise les identifiants enregistrés, en ligne comme hors ligne (FIX-9). Une déconnexion volontaire doit survivre au redémarrage. Les identifiants restent conservés pour pré-remplir l'écran de login (comportement AND-7 / FIX-8).

Android dispose déjà d'un drapeau persistant `session_active` dans `CredentialStore` (AND-7, posé par `HardwareUtils::setSessionActive`, utilisé par `PushPictureWorker`). iOS n'a pas d'équivalent.

## Fichiers

- `src/Application.cpp|h` (auto-login de lancement, `userLogout`, `resetAllData`, `homeLoaded`), `src/HardwareUtils.h|cpp`
- `android/HardwareUtils_Android.cpp`, `android/src/fr/calaos/calaoshome/CredentialStore.java`
- `ios/HardwareUtils.mm` (stockage iOS : `NSUserDefaults` de l'app, ou trousseau)
- `tests/` (harnais Qt Test)

## Implémentation

- Un état persistant « l'utilisateur s'est déconnecté » : posé par `userLogout()`, effacé par un login réussi (explicite) ; `resetAllData()` revient à l'état initial (comportement démo actuel inchangé).
- L'auto-login de lancement (et le login automatique au retour au premier plan sur iOS après un redémarrage) ne s'exécute pas tant que cet état est posé ; l'écran de login s'affiche pré-rempli.
- Une API commune dans `HardwareUtils` (lecture/écriture), implémentée sur Android en réutilisant `session_active` de `CredentialStore` si la sémantique colle (sinon un second drapeau), sur iOS dans les préférences de l'app, et sur desktop/hôte de test par `QSettings` ou un no-op cohérent.
- Le drapeau `session_active` (images de caméra dans les push, AND-7) reste cohérent avec ce nouvel état.

## Critères d'acceptation

- Tests unitaires : logout → « redémarrage » simulé (nouvelle instance de la logique de lancement lisant l'état persistant) ⇒ aucune tentative ; login réussi ⇒ l'état est effacé et le redémarrage suivant se connecte.
- Sur appareil (passage groupé) : Logout, tuer l'app, relancer ⇒ écran de login pré-rempli, pas de connexion ; Login ⇒ connexion, et les redémarrages suivants se connectent automatiquement. Idem hors ligne puis réseau.
- Builds Android, iOS, desktop OK.
