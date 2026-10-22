# Calaos Home — Privacy policy

*Last updated: October 2026* · [Version française plus bas](#calaos-home--politique-de-confidentialité)

Calaos Home is the open-source mobile app (Android and iOS) for the [Calaos](https://calaos.fr) home automation system. Its source code is available at <https://github.com/calaos/calaos_mobile> under the GNU General Public License v3 or later, with an exception that allows distribution through the App Store and Google Play.

The app has no account system, no advertising, no analytics and no tracking. It is a remote control for **your own Calaos server**.

## Data stored on your device

- The address of your Calaos server, your Calaos user name and password, and your app settings (language, favourites).
- The password is stored in the system's secure storage (Android Keystore-encrypted storage, iOS Keychain).
- To show notifications, the app keeps a push notification token and an unread counter for the iOS icon badge.

You can erase all of this at any time with *Settings → Reset all data* in the app, or by uninstalling it.

## Data sent to your Calaos server

The app talks to the Calaos server whose address you enter. It sends your user name and password to log in, the commands you trigger, and requests for the state of your home, your event history and your camera images. This server belongs to you (or to whoever installed it for you); the Calaos team has no access to it.

The connection uses HTTPS/secure WebSocket unless you enter an `http://` or `ws://` address.

## Push notifications

If you allow notifications, the app obtains a push token from Google Firebase Cloud Messaging (Android) or the Apple Push Notification service (iOS) and sends it **to your Calaos server only**.

When an event on your server must be notified, your server sends the token, the notification text and an event identifier to the Calaos push relay (`push.calaos.fr`), which forwards them to Google or Apple for delivery. The relay is operated by the Calaos project, is only used to deliver notifications, and keeps no detailed log of tokens or notification content.

When a notification refers to a camera picture, the app downloads that picture directly from your Calaos server, not through the relay.

Google and Apple process push tokens and notifications under their own privacy policies:
- Google: <https://policies.google.com/privacy>
- Apple: <https://www.apple.com/legal/privacy/>

Logging out in the app stops camera pictures from being downloaded for notifications. To stop notifications completely, disable them in your phone settings or uninstall the app.

## Children

Calaos Home is not directed at children and is intended for adults managing their home.

## Contact

Questions about this policy: team@calaos.fr

---

# Calaos Home — Politique de confidentialité

*Dernière mise à jour : octobre 2026*

Calaos Home est l'application mobile open source (Android et iOS) du système domotique [Calaos](https://calaos.fr). Son code source est disponible sur <https://github.com/calaos/calaos_mobile> sous licence GNU GPL v3 ou ultérieure, avec une exception qui permet sa distribution sur l'App Store et Google Play.

L'application n'a pas de compte utilisateur, pas de publicité, pas d'outil de statistiques ni de suivi. C'est une télécommande pour **votre propre serveur Calaos**.

## Données conservées sur votre appareil

- L'adresse de votre serveur Calaos, votre identifiant et votre mot de passe Calaos, et vos réglages (langue, favoris).
- Le mot de passe est conservé dans le stockage sécurisé du système (stockage chiffré par l'Android Keystore, trousseau iOS).
- Pour afficher les notifications, l'application garde un jeton de notification et un compteur de notifications non lues pour la pastille de l'icône iOS.

Vous pouvez tout effacer à tout moment avec *Réglages → Réinitialiser toutes les données* dans l'application, ou en la désinstallant.

## Données envoyées à votre serveur Calaos

L'application communique avec le serveur Calaos dont vous saisissez l'adresse. Elle lui envoie votre identifiant et votre mot de passe pour se connecter, les commandes que vous déclenchez, et les demandes d'état de votre maison, d'historique des événements et d'images de caméras. Ce serveur vous appartient (ou appartient à la personne qui l'a installé pour vous) ; l'équipe Calaos n'y a pas accès.

La connexion utilise HTTPS / WebSocket sécurisé, sauf si vous saisissez une adresse `http://` ou `ws://`.

## Notifications push

Si vous autorisez les notifications, l'application obtient un jeton de notification auprès de Google Firebase Cloud Messaging (Android) ou du service Apple Push Notification (iOS) et l'envoie **uniquement à votre serveur Calaos**.

Lorsqu'un événement de votre serveur doit être notifié, votre serveur envoie le jeton, le texte de la notification et un identifiant d'événement au relais de notifications Calaos (`push.calaos.fr`), qui les transmet à Google ou à Apple pour la distribution. Ce relais est exploité par le projet Calaos, ne sert qu'à distribuer les notifications et ne conserve pas de journal détaillé des jetons ni du contenu des notifications.

Quand une notification concerne une image de caméra, l'application télécharge cette image directement depuis votre serveur Calaos, pas par le relais.

Google et Apple traitent les jetons et les notifications selon leurs propres politiques de confidentialité :
- Google : <https://policies.google.com/privacy>
- Apple : <https://www.apple.com/legal/privacy/>

Se déconnecter dans l'application arrête le téléchargement des images de caméra pour les notifications. Pour arrêter complètement les notifications, désactivez-les dans les réglages du téléphone ou désinstallez l'application.

## Enfants

Calaos Home ne s'adresse pas aux enfants ; elle est destinée aux adultes qui gèrent leur maison.

## Contact

Questions sur cette politique : team@calaos.fr
