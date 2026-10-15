# T12 — TLS TOFU : pinning d'empreinte certificat ⚠️ ticket à risque

| Champ | Valeur |
|---|---|
| Phase | P2 — Robustesse réseau |
| Taille | L |
| Bloqué par | T08 (même fichier), T02 |
| Groupe de conflit | G-CONN + G-APP + qrc |
| Variantes | desktop + mobile |

## Contexte

Le client **ignore inconditionnellement toutes les erreurs de certificat TLS** — la connexion est trivialement MITM-able :
- `src/CalaosConnection.cpp:18-21` : `sslErrors(...)` → `reply->ignoreSslErrors();`
- `src/CalaosConnection.cpp:23-26` : `sslErrorsWebsocket(...)` → `wsocket->ignoreSslErrors();`
- `src/NetworkRequest.cpp:253-263` : `nmSslErrors` logge puis `reply->ignoreSslErrors();` (desktop)

Contrainte produit : beaucoup d'installations Calaos utilisent des **certificats auto-signés en LAN**. Une validation stricte casserait toutes les installs existantes. Décision retenue : **TOFU (Trust On First Use)** — à la première connexion, l'empreinte du certificat serveur est mémorisée (avec confirmation utilisateur si le cert est invalide) ; toute connexion ultérieure exige la même empreinte.

Secondairement, les credentials passent en query string GET sur certains chemins (`CalaosConnection.cpp:585-589` URL caméra v1 `&u=%3&p=%4`, `919-923` `getNotifPictureUrl` `?cn_user=…&cn_pass=…`).

## Fichiers

- **Nouveaux** : `src/TofuSslManager.h`, `src/TofuSslManager.cpp` → **à enregistrer dans `calaos.pri`** (SOURCES + HEADERS, car CalaosConnection est partagé)
- `src/CalaosConnection.cpp/.h` (slots SSL + flux de re-login)
- `src/NetworkRequest.cpp/.h` (desktop — noter que `NetworkRequest::setCertificate` existe déjà ligne 34 pour un CA pinné : le conserver comme mécanisme complémentaire)
- `src/Application.cpp/.h` (exposition QML des signaux de confiance)
- **Nouveaux QML** : `qml/mobile/DialogSslTrust.qml` → **qml_mobile.qrc** ; `qml/desktop/DialogSslTrust.qml` (réutiliser `qml/desktop/Dialog.qml` comme base) → **qml_desktop.qrc**
- **Nouveau test** : `tests/tst_tofussl/` (+ `tests/tests.pro`) avec certificats PEM de fixture dans `tests/data/` (générés une fois via openssl, commités)

## Implémentation

### TofuSslManager

```cpp
enum class CheckResult { Trusted, Unknown, Mismatch };
CheckResult check(const QString &hostPort, const QSslCertificate &peer);
void trust(const QString &hostPort, const QSslCertificate &peer); // stocke l'empreinte
```
- Empreinte : `QSslCertificate::digest(QCryptographicHash::Sha256).toHex()`.
- Persistance : clé `tofu/<host:port>` via `HardwareUtils::Instance()->setConfigOption()/getConfigOption()` (virtuels, `src/HardwareUtils.h:52-53` — QSettings en base, XML sur desktop : déjà en place, pas de nouveau store).

### Flux dans les slots SSL

- **Certificat valide** (chaîne CA OK) → `sslErrors` n'est pas appelé → comportement inchangé, aucun dialog.
- **Empreinte stockée == empreinte du peer** → `ignoreSslErrors()` (c'est le seul cas où on l'appelle encore).
- **Aucune empreinte stockée** → **abort** de la connexion + émission `sslTrustRequest(host, fingerprint, subjectCN, issuer, expiry)` → dialog QML → si l'utilisateur accepte : `trust()` puis relancer `login()` ; s'il refuse : rester déconnecté proprement (état NotConnected).
- **Mismatch** → abort + `sslTrustMismatch(...)` → dialog explicite avec option « faire confiance au nouveau certificat » (écrase l'ancienne empreinte) — formulation anxiogène volontaire (le cert a changé : régénération légitime OU attaque).

### Points d'attention

- **WebSocket** : `QWebSocket::ignoreSslErrors()` doit être appelé de façon **synchrone dans le slot** `sslErrorsWebsocket`. Pas d'attente asynchrone possible dans le slot → pattern « abort maintenant, re-login après acceptation du dialog ». Le certificat peer est accessible via les `QSslError` reçus (`error.certificate()`).
- **Trois chemins à couvrir** : accessManager HTTP + accessManagerCam (CalaosConnection), WebSocket, et `NetworkRequest` (desktop : météo/CalaosOsAPI — pour les URLs localhost/HTTP ce chemin est rarement TLS, mais le handler doit suivre la même logique).
- Le mode démo (`demo.calaos.fr`, cert valide) ne doit déclencher **aucun** dialog.
- **Credentials GET→POST** : basculer les requêtes qui passent `cn_user`/`cn_pass` en query string vers POST avec corps JSON quand l'API serveur le permet (les commandes JSON existantes sont déjà en POST — s'aligner). Conserver le GET v1 caméra si le protocole v1 l'exige, avec un commentaire justificatif.

### Test

`tests/tst_tofussl/` : logique pure compare/store de TofuSslManager avec 2 certs PEM de fixture — premier contact (Unknown), après trust (Trusted), cert différent (Mismatch), re-trust (Trusted).

## Critères d'acceptation

- `grep -n "ignoreSslErrors" src/` : plus **aucun** appel inconditionnel — uniquement dans la branche « empreinte vérifiée » de TofuSslManager/slots.
- Test unitaire vert.
- Scénario manuel complet (voir ci-dessous) sur desktop **et** mobile.

## Vérification (manuelle, obligatoire — desktop ET mobile)

1. Serveur LAN avec cert auto-signé, première connexion → dialog affiché **une seule fois** ; accepter → connecté (WS + HTTP + images caméra fonctionnent).
2. Redémarrer l'app → connexion directe, aucun dialog.
3. Régénérer le certificat serveur → connexion refusée + dialog mismatch ; accepter → reconnecté.
4. Refuser le dialog → l'app reste déconnectée proprement (pas de boucle de retry sur le dialog).
5. Mode démo (`demo.calaos.fr`, cert valide) → aucun dialog.
6. Desktop : chemins `NetworkRequest` (météo, API OS) toujours fonctionnels.
