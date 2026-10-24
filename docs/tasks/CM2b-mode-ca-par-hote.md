# CM2b — Mode d'épinglage par hôte : un hôte passé à un certificat CA n'est plus épinglé

| Champ | Valeur |
|---|---|
| Origine | Revue de la PR #40 (CM2), suite du correctif F1 |
| Taille | S |
| Bloqué par | CM2 |
| Groupe de conflit | G-CONN + G-APP + lang |
| Variantes | desktop + mobile |

## Contexte

Depuis F1 (revue CM2), un hôte épinglé qui présente un certificat valide pour une CA avec une autre clé déclenche la question « clé changée », et l'acceptation enregistrait la nouvelle clé comme pin. Une box passée d'un certificat auto-signé à Let's Encrypt (jointe par son nom DDNS) était alors redemandée à chaque renouvellement certbot qui change la clé (le comportement par défaut, sans `--reuse-key`).

## Décisions

- **Deux modes par hôte**, déduits du contenu de `tls/pins` (aucun nouveau champ stocké) : *épinglé* (une entrée) ou *CA* (pas d'entrée, la validation Qt suffit).
- **Premier certificat valide pour une CA** (aucune erreur TLS) : accepté sans question et sans pin (déjà le cas avec CM2, vérifié par `wsCaValidUnpinnedConnects`).
- **Auto-signé ou non validé** : épinglé comme avec CM2.
- **Hôte épinglé qui présente un certificat valide avec une autre clé** : toujours la question, jamais de bascule silencieuse (formulation ambre « clé changée », second appui sur mobile). `pendingTrust` retient `caValid` (= aucune erreur TLS) ; si l'utilisateur accepte, le pin de l'hôte est **supprimé** (`forget`) au lieu d'être remplacé : l'hôte passe en mode CA, les renouvellements suivants passent sans question. Refuser garde le pin.
- **Formulation** : quand `caValid`, le texte indique que la box présente maintenant un certificat d'une autorité publique et qu'accepter arrête l'épinglage de cet hôte (FR/DE).
- **Copies** (App Group iOS, `CredentialStore` Android) : suivent via `pinsChanged` comme avant ; sans pin, l'extension iOS et le worker Android s'en remettent à la validation système (aucun changement de code natif).
- **Cache de connexions** : `pinsChanged` vide aussi le cache du QNAM de la fabrique QML et celui des requêtes `NetworkRequest` (gestionnaire éventuellement partagé avec `CalaosOsAPI`, sans effet de bord).

## Fichiers

- `src/CalaosConnection.{h,cpp}` (`caValid` dans la question, `forget` à l'acceptation), `src/TlsPinning.h` (commentaire), `src/Application.cpp`, `src/NetworkRequest.cpp`.
- `qml/mobile/TlsTrustSheet.qml`, `qml/desktop/DialogTlsTrust.qml`, `lang/calaos_{fr,de}.{ts,qm}`.
- `tests/tst_tlspinning/` (deuxième feuille signée par la CA de test).

## Critères d'acceptation

- `tst_tlspinning` : épinglé sur A puis feuille CA avec une autre clé → question `keyChanged` + `caValid` ; acceptation → plus de pin, connecté ; feuille CA renouvelée avec une clé B' sur le même `hôte:port` → connexion sans question ; refus → le pin reste ; un hôte auto-signé reste épinglé (`caValid` faux).
- `make -k check` vert (hors tests instables connus), builds desktop, simulateur iOS, Android Debug.

## Vérification manuelle

1. Box auto-signée épinglée, puis passage à Let's Encrypt joint par le nom DDNS → « Certificat modifié ! » avec la mention de l'autorité publique ; accepter (deux appuis sur mobile) → connecté, l'hôte disparaît de la liste des certificats de confiance.
2. Renouvellement certbot avec nouvelle clé → aucune question ; image de push toujours affichée (iOS, Android).
