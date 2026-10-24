# AP2 — Nom d'appareil dans `register_push`, identifiants en HTTP

| Champ | Valeur |
|---|---|
| Phase | P-ADMIN — Administration de la box, phase 2 (téléphone approbateur) |
| Taille | S |
| Bloqué par | — (∥ AP1a côté calaos-container ; AP1b côté calaos_base consomme le champ) |
| Groupe de conflit | G-CONN + G-ANDROID + G-IOS |
| Variantes | mobile (iOS, Android) ; desktop neutre (code partagé) |

## Contexte

Plan admin de calaos_base (`docs/admin/PLAN.md`, « Second facteur : code par push vers un téléphone approbateur (phase 2) », ligne AP2) : un admin déverrouillé choisit son téléphone approbateur parmi les jetons `register_push`, qui gagnent un nom d'appareil `device_name` (stocké dans `push_tokens` par AP1b). Le daemon envoie un code à ce seul jeton. Pas de secret d'appareil (décision de Raoul) : rien de plus n'est stocké sur le téléphone, une rotation du jeton est un ré-enrôlement côté webapp.

Le plan signale aussi `register_push` envoyé sans identifiants en mode HTTP (un échec de login par session, qui pollue `LoginThrottle`). Vérifié le 2026-10-23 : déjà corrigé par FIX-3 (`03541bd`, `CalaosConnection::registerPushToken()` ajoute `cn_user`/`cn_pass` en HTTP ; `tst_pushnotif` le vérifie contre un faux `api.php` qui refuse en 400 sans identifiants). Le constat du plan date de l'app 3.1.x des stores.

Limite iOS : depuis iOS 16, sans l'entitlement `com.apple.developer.device-information.user-assigned-device-name`, tous les iPhone se déclarent « iPhone ». AP1b et WA3 doivent donc afficher de quoi les distinguer (plateforme, date d'enregistrement, dernière activité). Décision en attente avec Raoul : demander l'entitlement ou ajouter un nom saisi dans l'app.

## Fichiers

- `src/CalaosConnection.h|cpp` (`PushFormat::registration`), `src/JsonKeys.h`
- `src/HardwareUtils.h|cpp` (`getDeviceName()`, nom d'hôte par défaut)
- `ios/HardwareUtils_iOS.h`, `ios/HardwareUtils.mm`
- `android/HardwareUtils_Android.h|cpp`, `android/src/fr/calaos/calaoshome/HardwareUtils.java`
- `tests/tst_pushnotif/`

## Implémentation

- `register_push` porte `device_name` (dans `data` en WebSocket, à la racine en HTTP, comme les autres champs) ; champ omis si le nom est vide ; un calaos_server ancien l'ignore (`Params` lus par clé).
- Source du nom : iOS `UIDevice.currentDevice.name` (depuis iOS 16 le modèle générique « iPhone » sans l'entitlement `com.apple.developer.device-information.user-assigned-device-name`, non demandé) ; Android `Settings.Global.DEVICE_NAME`, sinon `Build.MANUFACTURER` + `Build.MODEL` ; desktop et tests : nom d'hôte.
- Pas de réglage pour surcharger le nom dans cette version.

## Critères d'acceptation

- Tests Qt (`tst_pushnotif`) : charge utile avec et sans nom, nom présent en HTTP (avec identifiants) et en WebSocket (sans identifiants) contre les faux serveurs en processus ; aucun jeton ni mot de passe en clair dans les logs.
- `make check` vert ; builds desktop, iOS simulateur et Android Debug.
- Sur appareil (au prochain passage, serveur AP1b) : le nom du téléphone apparaît dans la liste des approbateurs de la webapp.
