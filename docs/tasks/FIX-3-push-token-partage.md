# FIX-3 — Push : tap hors connexion, rotation du jeton, jeton dans les logs

| Champ | Valeur |
|---|---|
| Phase | P-FIX — Petits bugs constatés pendant P-AND |
| Taille | M |
| Bloqué par | AND-4 |
| Groupe de conflit | G-CONN + G-ANDROID |
| Variantes | mobile (iOS, Android) |

## Contexte

Relevé par la revue d'AND-4 (2026-10-16), dans le code partagé `src/` (iOS a les mêmes défauts latents) :

1. **Tap sur une notification pendant une déconnexion perdu** : `HardwareUtilsAndroid::handleNotificationOpened` positionne `startedWithNotif` quand l'app n'est pas connectée, mais `Application::homeLoaded` ne le consomme que si `!startedWithOptHandled`, flag remis à zéro uniquement par `applicationWillResignActive`, qu'Android n'émet jamais. Après le premier login du processus, un tap pendant une reconnexion est ignoré.
2. **Rotation du jeton** : un nouveau jeton (`onNewToken` → `setDeviceToken`) ne met à jour que le membre ; `register_push` n'est envoyé qu'au login (`src/CalaosConnection.cpp`, blocs `#if defined(Q_OS_ANDROID)` ~437-454 et ~573-588), le serveur garde l'ancien jeton jusqu'à la reconnexion suivante.
3. **Jeton dans les logs** : `CalaosConnection::credentialKeys()` ne masque pas `token`, donc le log debug de `sendHttp` (repli HTTP) l'affiche en clair.

## Fichiers

- `src/Application.cpp|h`, `src/CalaosConnection.cpp|h`, `src/HardwareUtils.cpp|h`
- `android/HardwareUtils_Android.cpp|h`, `ios/HardwareUtils.mm` si nécessaire

## Implémentation

1. Un tap différé est ouvert au prochain passage à l'état connecté (une seule fois), sur Android comme sur iOS.
2. `HardwareUtils` émet un signal quand le jeton change ; si la connexion est établie, `CalaosConnection` renvoie `register_push` (sans doublon au login).
3. Ajouter la clé du jeton à `credentialKeys()` (constante de `JsonKeys.h` si elle existe).

## Critères d'acceptation

- Tests unitaires Qt Test (harnais `tests/`, cf. T02) pour la logique du tap différé et du renvoi de `register_push` si elle est testable sans plateforme ; sinon justification.
- `tools/build_ios.sh Debug` et `tools/build_android.sh Debug` passent ; build desktop non affecté (code sous garde ou neutre).
- Vérification sur appareil (au prochain passage) : tap sur une notification pendant une reconnexion → l'événement s'ouvre après reconnexion.
- Aucun jeton complet dans les logs (y compris repli HTTP en debug).
