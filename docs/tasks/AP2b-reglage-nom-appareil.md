# AP2b — Réglage « Nom de cet appareil », renvoyé par `register_push`

| Champ | Valeur |
|---|---|
| Phase | P-ADMIN — Administration de la box, phase 2 (téléphone approbateur) |
| Taille | S |
| Bloqué par | AP2 |
| Groupe de conflit | G-CONN + G-QML-MOBILE + G-QML-DESKTOP |
| Variantes | mobile (iOS, Android) ; desktop (réglage présent, sans effet tant que le kiosque n'a pas de jeton push) |

## Contexte

AP2 envoie `device_name` avec `register_push` (`HardwareUtils::getDeviceName()`). Depuis iOS 16, sans l'entitlement `com.apple.developer.device-information.user-assigned-device-name`, `UIDevice.name` vaut « iPhone » pour tous les iPhone : l'admin ne peut pas distinguer les téléphones en choisissant l'approbateur. Décision de Raoul (point laissé ouvert par AP2) : un nom saisi dans l'app plutôt que l'entitlement.

## Fichiers

- `src/HardwareUtils.h|cpp` (surcharge du nom, assainissement, signal `deviceNameChanged()`)
- `src/CalaosConnection.h|cpp` (`PushRegistration` sur le couple jeton + nom, renvoi sur renommage)
- `src/Application.cpp` (`resetAllData()`)
- `qml/mobile/SettingsView.qml`, `qml/desktop/ConfigUserInfoView.qml`
- `lang/calaos_fr.ts|qm`, `lang/calaos_de.ts|qm`
- `tests/tst_pushnotif/`

## Implémentation

- Réglage « Nom de cet appareil » : mobile dans Réglages (champ texte prérempli, sous « Connexion »), desktop dans « Informations utilisateur » (bouton « Modifier », clavier virtuel). Ligne d'aide : « Ce nom permet de reconnaître ce téléphone dans l'administration de la box ».
- Stockage local : option de config `device_name` (QSettings `config/device_name` sur mobile, `local_config.xml` sur desktop), via `HardwareUtils::setConfigOption()`. Vide, ou égal au nom de la plateforme, = pas de surcharge : le nom de la plateforme est suivi.
- Assainissement (`HardwareUtils::sanitizeDeviceName()`) : caractères de contrôle (Cc) et séparateurs de ligne/paragraphe remplacés par une espace, contrôles bidi et caractères invisibles retirés (U+061C, U+200B, U+200E, U+200F, U+202A–U+202E, U+2060, U+2066–U+2069, U+FEFF ; ZWJ/ZWNJ, sélecteurs de variante et caractères d'étiquette gardés pour les emoji), espaces internes fusionnées en une seule, bords supprimés, demi-paires de substitution isolées retirées ; plafond de 64 unités UTF-16 (ce que compte `maximumLength` en QML), coupé sur une limite de graphème (jamais au milieu d'une paire de substitution ni d'une séquence emoji ZWJ).
- `register_push` envoie `HardwareUtils::effectiveDeviceName()` (surcharge, sinon `getDeviceName()`, toujours assaini). `PushRegistration::shouldRegister()` compare le jeton **et** le nom : un renommage émet `deviceNameChanged()`, `CalaosConnection` renvoie `register_push` tout de suite sur le transport courant (WebSocket ou HTTP avec identifiants) si une session et un jeton existent, sinon au prochain login. Un nom inchangé après assainissement n'émet rien et n'envoie rien.
- Mobile : ce qui est tapé est aussi enregistré en quittant la page (retour, barre du bas, Déconnexion, Reset all data, passage en arrière-plan), pas seulement sur « Terminé ». Desktop : bouton « Modifier » soumis au verrou des réglages, comme les identifiants.
- « Reset all data » efface la surcharge, après le `logout()` (l'ancienne box ne reçoit pas le nom par défaut).

## Hors périmètre

- Côté serveur/webapp (AP1b, WA3) : afficher le nom isolé (`<bdi>` ou `unicode-bidi: isolate`) ; le nom de la plateforme est lui aussi choisi par l'utilisateur, et les versions précédentes de l'app (ou d'autres clients) l'envoient sans filtrage.
- L'entitlement iOS du nom utilisateur de l'appareil.
- Le kiosque desktop n'envoie pas `register_push` (pas de jeton push) : le réglage y est stocké mais sans effet aujourd'hui.

## Critères d'acceptation

- Tests Qt (`tst_pushnotif`) : valeur par défaut, assainissement (bords, contrôles, espaces, plafond de 64 avec paires de substitution et séquences ZWJ), signal émis seulement sur changement, renvoi d'un second `register_push` avec le nouveau nom en HTTP et en WebSocket contre les faux serveurs, rien si inchangé, au login suivant si déconnecté.
- `make check` vert ; builds desktop, iOS simulateur et Android Debug.
- Sur appareil : le nom saisi apparaît dans la liste des approbateurs de la webapp (serveur AP1b).
