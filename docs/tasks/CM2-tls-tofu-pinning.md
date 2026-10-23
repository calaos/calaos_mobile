# CM2 — TLS TOFU : épinglage de la clé du certificat, par hôte, avec re-confirmation

| Champ | Valeur |
|---|---|
| Origine | Plan admin de calaos_base (`docs/admin/PLAN.md`, phase 2, ticket CM2) |
| Taille | L |
| Bloqué par | — |
| Groupe de conflit | G-CONN + G-APP + G-ANDROID + G-IOS + qrc + lang |
| Variantes | desktop + mobile (+ worker push Android, extension iOS) |
| Remplace | T12 (écarté le 2026-08-10) |

## Contexte

Le client ignorait inconditionnellement les erreurs TLS (`ignoreSslErrors()` dans `CalaosConnection` et `NetworkRequest`) : un MITM sur le LAN captait les identifiants et le token push. T12 proposait un TOFU, écarté parce qu'un renouvellement du certificat serveur aurait coupé les clients. CM2 le reprend **avec une nouvelle confirmation explicite** quand la clé change, jamais d'acceptation silencieuse ni de coupure définitive.

## Décisions

- **Ce qui est épinglé** : le SHA-256 du *SubjectPublicKeyInfo* du certificat feuille (pin SPKI), pas le certificat entier. Un renouvellement qui garde la clé (certificat auto-signé régénéré avec la même clé, certbot `--reuse-key`) ne redemande rien ; une nouvelle clé redemande toujours. Les octets SPKI sont extraits par un parcours DER maison (`TlsPinning::subjectPublicKeyInfo`), identique quel que soit le backend TLS (OpenSSL, Secure Transport), et égal à `PublicKey.getEncoded()` côté Java et au parcours DER de l'extension iOS.
- **Clé du pin** : `hôte:port` (hôte en minuscules, IPv6 entre crochets, port par défaut explicite : `wss://` et `https://` partagent 443). IP LAN, nom DDNS et IP overlay sont trois hôtes distincts.
- **Certificat valide pour une CA publique** (validation Qt réussie, nom conforme) : accepté sans pin, aucun dialogue.
- **Erreurs surmontables par un pin** (liste blanche explicite, `TlsPinning::isOverridable`) : auto-signé, émetteur inconnu ou non approuvé, CA invalide, usage invalide, nom différent, **expiré** ou pas encore valide (le pin porte sur la clé, et beaucoup de box tournent avec un vieux certificat auto-signé). Tout le reste (révoqué, liste noire, signature invalide, pas de certificat, erreur inconnue) est refusé.
- **Seulement les erreurs de ce handshake** : `ignoreSslErrors(errors)`, jamais la version sans argument. QNAM garde le drapeau sur son canal en cache : un ignore global laissait passer un handshake suivant avec une autre clé (corps de requête et identifiants envoyés). Oublier un pin ou tout réinitialiser vide aussi le cache de connexions.
- **Seul le certificat feuille** est pris en compte (jamais un certificat porté par une erreur, qui peut être un intermédiaire) ; feuille absente = refus.
- **Redirections** : `SameOriginRedirectPolicy` sur les QNAM de `CalaosConnection`, aucune redirection suivie par l'extension iOS ni le worker Android (les identifiants ne partent jamais vers un autre hôte).
- **Stockage** : QSettings `tls/pins` (map `hôte:port` → hex), mobile et desktop. Copie pour le worker Android (`CredentialStore`, `tls_pins`) et l'extension iOS (App Group, `calaos.tls_pins`). « Réinitialiser toutes les données » efface tout ; Réglages mobile liste les certificats de confiance avec « Oublier ».

## Flux

1. Handshake → `sslErrors` → `TlsPinning::decide()` : `PinMatch` → `ignoreSslErrors()` (seul appel restant) ; sinon la tentative échoue.
2. Clé inconnue ou changée : `ReconnectPolicy::TrustFailure` (abandon, aucun retry automatique, pas de repli HTTP depuis le websocket), et **une** question à l'utilisateur (`tlsTrustRequested`) : empreinte SPKI, empreinte du certificat, sujet, émetteur, expiration. Formulation alarmante si la clé a changé, ou si le certificat est suspect (hôte joint par nom DNS — ni IP ni `.local` —, ou chaîne valide émise pour un autre nom) ; dans ces cas « Faire confiance » est atténué et demande un second appui sur mobile. Tant qu'une question est ouverte, un autre certificat ne la remplace pas : il est refusé pour cette tentative.
3. Accepter → pin enregistré, nouvelle tentative immédiate. Refuser → reste déconnecté, alerte claire ; le même certificat est ensuite refusé **sans question** pour les connexions lancées par l'app (lancement, retour au premier plan iOS), jusqu'à un appui explicite sur « Connexion ». Déconnexion pendant la question → la question est annulée.
4. Chemins sans utilisateur (images QML, client du daemon desktop, worker Android, extension iOS) : vérification seule, jamais de question.

## Limites connues

- **Normalisation de la clé d'hôte** : nom en forme ACE (punycode) partout (`QUrl::FullyEncoded`, `IDN.toASCII` en Java, `NSURLProtectionSpace.host` sur iOS), IPv6 entre crochets. Une forme écrite différemment (IPv6 non canonique, nom avec point final) donne une autre clé : la question revient, ou l'image push est omise. Échec fermé, acceptable.
- **SPKI côté Java** : `PublicKey.getEncoded()` ré-encode la clé ; pour RSA et EC c'est le DER canonique, identique aux octets du certificat. Une clé encodée de façon non canonique dans le certificat donnerait une empreinte différente côté worker : image omise (échec fermé).
- **Let's Encrypt joint par IP LAN** : certbot change la clé à chaque renouvellement sauf `--reuse-key` : nouvelle question tous les 60 à 90 jours. Joint par son nom DDNS, il est valide publiquement : aucune question.
- **iOS, ATS** : l'extension n'est pas soumise à ATS pour une IP ou un nom `.local` ; une box auto-signée jointe par un nom DNS peut demander une exception ATS (à vérifier sur appareil).
- **Desktop** : après un refus, le kiosque reste déconnecté jusqu'au redémarrage (pas d'écran de login) ; pas de liste « Oublier » sur desktop.

## Fichiers

- Nouveaux : `src/TlsPinning.h/.cpp` (calaos.pri + `cmake/CalaosHomeSources.cmake`), `qml/mobile/TlsTrustSheet.qml`, `qml/desktop/DialogTlsTrust.qml`, `tests/tst_tlspinning/`, `tests/data/tls_*.pem`.
- Modifiés : `CalaosConnection` (slots SSL, `TrustFailure`), `NetworkRequest`, `Application` (signaux, fabrique NAM QML, réglages, reset), `HardwareUtils` (+ iOS, Android : `setTlsPins`), `CredentialStore.java`, `PushPictureWorker.java`, `ios/PushNotif/NotificationService.m`, `BottomSheet.qml` (`closeOnLogout`), `SettingsView.qml`, traductions FR/DE.

## Critères d'acceptation

- `grep -rn ignoreSslErrors src/` : uniquement derrière une correspondance de pin.
- `tst_tlspinning` vert : clé d'hôte, SPKI = openssl, renouvellement à clé constante, décision (CA publique, premier contact, correspondance, clé changée, erreurs non surmontables), isolation par hôte, persistance ; handshake websocket et HTTP réels sur 127.0.0.1 (question unique, pas de retry, acceptation → connecté, clé changée → nouvelle question, refus sans boucle, déconnexion qui annule).
- `tst_reconnectpolicy` : `TrustFailure` ne relance rien seul.
- Builds desktop, simulateur iOS (avec PushNotif), Android Debug.

## Vérification manuelle (desktop ET mobile, appareil réel)

1. Box à certificat auto-signé, première connexion → feuille/dialogue une seule fois ; comparer l'empreinte (`openssl x509 -in cert.pem -pubkey -noout | openssl pkey -pubin -outform der | openssl dgst -sha256`) ; accepter → connecté (WS, HTTP, caméras, image de l'historique).
2. Redémarrer l'app → connexion directe.
3. Régénérer le certificat avec une nouvelle clé → « Certificat modifié ! » ; accepter → reconnecté. Avec la même clé → aucune question.
4. Refuser → reste déconnecté, alerte ; retour au premier plan : pas de nouvelle question ; « Connexion » → question.
5. Mode démo et serveur à certificat public → aucune question.
6. Push avec image : affichée après acceptation dans l'app ; texte seul après « Oublier » (Android et iOS). iOS : tester aussi une box auto-signée jointe par un nom DNS (ATS, voir `doc/ios.md`).
7. Migration : après mise à jour, une box auto-signée demande une fois.
