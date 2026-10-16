# FIX-4 — Android : détecter les changements de réseau en cours d'exécution

| Champ | Valeur |
|---|---|
| Phase | P-FIX — Petits bugs constatés pendant P-AND |
| Taille | S |
| Bloqué par | FIX-3 |
| Groupe de conflit | G-ANDROID |
| Variantes | mobile (Android) |

## Contexte

Relevé pendant AND-3 : Android n'émet jamais `HardwareUtils::networkStatusChanged`. `getNetworkStatus()` n'est lu qu'au login ; un passage Wi-Fi → 4G → mode avion pendant que l'app tourne n'est pas vu (iOS le détecte via Reachability, `ios/HardwareUtils.mm`).

## Fichiers

- `android/HardwareUtils_Android.cpp|h`, `android/src/fr/calaos/calaoshome/HardwareUtils.java` (ou solution Qt)
- `src/Application.cpp` (consommateur de `networkStatusChanged`) en lecture

## Implémentation

Émettre `networkStatusChanged` sur Android, de préférence avec l'API Qt `QNetworkInformation` (backend Android natif de Qt) plutôt qu'un `NetworkCallback` Java maison ; garder `getNetworkStatus()` cohérent avec l'état émis. Comparer avec le comportement iOS (quand `Application` se reconnecte / se met en pause) pour obtenir la même logique.

## Critères d'acceptation

- Sur le téléphone : couper/rétablir le Wi-Fi ou passer en mode avion pendant que l'app tourne → `networkStatusChanged` émis (log), l'app se comporte comme sur iOS (reconnexion au retour du réseau).
- Pas de crash, logcat propre.
