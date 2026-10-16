# BOARD — Backlog de refactoring Calaos Home

Suivi du backlog de refactoring (robustesse, maintenabilité, bonnes pratiques). Chaque ticket est spécifié dans [`docs/tasks/`](tasks/) et est auto-porteur : il peut être confié tel quel à un agent de développement autonome.

Chantier « devices / climate » : tickets MOB-1 à MOB-6 (widget, item de liste, bottom sheet et dialogue de réglage climate) suivis dans le board de calaos_base (`docs/devices/BOARD.md`).
FIX-MOB-1 (sheets et dialogues fermés à la déconnexion) : suivi dans le même board de calaos_base. CLIM-9 (libellé « Sonde hors limites ») : idem.

## Règles d'utilisation

- **Le statut vit ici et uniquement ici** (les fichiers de ticket sont des specs immuables). Statuts : `À faire` → `En cours` → `En revue` → `Terminé`.
- Un agent qui prend un ticket passe son statut à `En cours` (une ligne modifiée dans ce fichier, dans le même commit/branche que son travail).
- Un ticket ne peut être pris que si **tous ses tickets « Bloqué par » sont `Terminé`**.
- Deux tickets ne peuvent être `En cours` simultanément que s'ils appartiennent à des **groupes de conflit différents** (voir colonne Groupe). Exception tolérée : les appends sur `calaos.pri`, `tests/tests.pro` et les `.qrc` (conflits triviaux à résoudre au merge).
- `Terminé` = critères d'acceptation du ticket validés + `make check` vert + CI verte.
- Rappels systématiques pour tout ticket : nouveau fichier C++ → enregistrer dans `calaos.pri` (partagé) ou `desktop.pro`/`mobile.pro` ; nouveau QML → enregistrer dans le bon `.qrc` ; toujours considérer les **deux variantes** de build (`CALAOS_DESKTOP` / `CALAOS_MOBILE`).

## P0 — Fondations (séquentiel, barrière : CI verte avant P1)

| ID | Titre | Taille | Bloqué par | Groupe | Statut |
|---|---|---|---|---|---|
| [T01](tasks/T01-build-cpp17.md) | Hygiène build : C++17 et globs lupdate | S | — | G-BUILD | Terminé |
| [T02](tasks/T02-harnais-qt-test.md) | Harnais Qt Test minimal | M | T01 | G-BUILD | Terminé |
| [T03](tasks/T03-ci-pr.md) | CI de PR : build + tests + qmllint | M | T01, T02 | G-BUILD | Terminé |

## P1 — Bugs critiques

| ID | Titre | Taille | Bloqué par | Groupe | Statut |
|---|---|---|---|---|---|
| [T04](tasks/T04-roomfiltermodel-resetcache.md) | RoomFilterModel : resetCache incomplet + cast non vérifié | M | T02 | G-MODELS | Terminé |
| [T05](tasks/T05-eventlogmodel-output-io.md) | EventLogModel : résolution des IO output cassée | S | T02 | G-MODELS | Terminé |
| [T06](tasks/T06-homemodel-oncache.md) | HomeModel : onCache jamais vidé (lumières fantômes) | S | T02 | G-MODELS | Terminé |
| [T07](tasks/T07-bugs-qml-enums.md) | Bundle bugs QML : enums, iconState, densité², action flux | S | T03 | G-QML-SHARED | Terminé |
| [T08](tasks/T08-calaosconnection-fixes.md) | CalaosConnection : correctifs ciblés pré-refactoring | M | T02 | G-CONN | Terminé |
| [T09](tasks/T09-weatherinfo-fuite.md) | WeatherInfo : fuite mémoire forecastDataClear | S | — | indépendant | Terminé |
| [T10](tasks/T10-gardes-nan-qml.md) | Bundle gardes NaN / division par zéro QML | S | T03 | QML (disjoint T07) | Terminé |

Note : T04/T05/T06 sont parallélisables entre eux (fichiers disjoints, seule collision = append de `tests/tests.pro`).

## P2 — Robustesse réseau & sécurité

| ID | Titre | Taille | Bloqué par | Groupe | Statut |
|---|---|---|---|---|---|
| [T11](tasks/T11-parsing-sur-src.md) | Sweep parsing sûr : toInt/toDouble + null-checks | M | T04, T05, T06 | G-MODELS + G-COMMON | Terminé |
| [T13](tasks/T13-timeouts-http.md) | Timeouts HTTP + durcissement long-poll | S | T08 | G-CONN | Terminé |
| [T14](tasks/T14-reconnexion-backoff.md) | ⚠️ Machine à états de reconnexion + backoff | L | T13 | G-CONN + G-APP | Terminé |
| [T15](tasks/T15-camera-polling-imageproviders.md) | Polling caméra maîtrisé + image providers thread-safe | M | T11 | G-MODELS | Terminé |

## P3 — Découplage C++

| ID | Titre | Taille | Bloqué par | Groupe | Statut |
|---|---|---|---|---|---|
| [T16](tasks/T16-iotyperegistry.md) | IOTypeRegistry : source de vérité unique des types d'IO | L | T11, T15, T02 | G-MODELS + G-COMMON | Terminé |
| [T17](tasks/T17-calaoseventdecoder.md) | Extraction du décodeur d'événements | L | T14 | G-CONN | Terminé |
| [T18](tasks/T18-decouplage-roommodel-connection.md) | Découpler RoomModel → CalaosConnection | S | T16, T17 | G-MODELS + G-CONN | Terminé |
| [T19](tasks/T19-favoritesmodel-favtypes.md) | FavoritesModel : types non-FavIO (TODO!) | M | — | indépendant | Terminé |
| [T20](tasks/T20-qqmlhelpers-garde-egalite.md) | qqmlhelpers : garde d'égalité dans les setters | S | T16 | G-COMMON | Terminé |
| [T21](tasks/T21-jsonkeys.md) | Constantes de clés JSON (sweep src/) | M | T16, T17, T18 | sweep src — sérialisé | Terminé |
| [T22](tasks/T22-connect-modernes.md) | connect() modernes : SIGNAL/SLOT → pointeurs (sweep src/) | M | T21 | sweep src — sérialisé | Terminé |

## P4 — Consolidation QML

| ID | Titre | Taille | Bloqué par | Groupe | Statut |
|---|---|---|---|---|---|
| [T23](tasks/T23-iobinarydevice.md) | Widget IO binaire générique (5 widgets fusionnés) | M | T07 | G-QML-SHARED | Terminé |
| [T24](tasks/T24-delegate-map-qmldir.md) | ItemListView : delegate map + qmldir complet | M | T23 | G-QML-SHARED | Terminé |
| [T25](tasks/T25-pagescaffold-desktop.md) | PageScaffold desktop : squelette header/footer | M | T07 | G-QML-DESKTOP | Terminé |
| [T26](tasks/T26-mediawebview-onglets.md) | MediaWebView : source de vérité unique des onglets | M | T03 | G-QML-DESKTOP | Terminé |
| [T27](tasks/T27-sweep-units-dp.md) | Unification sizing : Units.dp partout (sweep) | L | T10, T23-T26 | G-QML-SWEEP — sérialisé | Terminé |

## P5 — Polish

| ID | Titre | Taille | Bloqué par | Groupe | Statut |
|---|---|---|---|---|---|
| [T29](tasks/T29-imports-qml-modernes.md) | Modernisation imports QML (Qt5Compat → MultiEffect) | M | T27 | G-QML-SWEEP | Terminé |
| [T30](tasks/T30-nettoyage-final.md) | Nettoyage final : logs, placeholders, formatters | S | T29 | G-QML-SWEEP | Terminé |

## P6 — Suite du découplage

| ID | Titre | Taille | Bloqué par | Groupe | Statut |
|---|---|---|---|---|---|
| [T31](tasks/T31-decouplage-iobase-dialogue.md) | Découpler IOBase → couche dialogue (askStateText) | S | T18 | G-MODELS | En revue |

Ouvert après P3 : c'est le dernier couplage inverse du modèle, celui qui oblige
encore trois tests à lier `HardwareUtils` ou une doublure de `RoomModel`.

## P-AND — Android Qt 6

La version Android n'a jamais fonctionné en Qt 6. Analyse et plan validés par Raoul le 2026-10-16 (push via SDK Firebase Java, tests sur téléphone USB, branche `feat/android-qt6`). Skills dédiés dans `.claude/skills/` : `qt6-android-build`, `qt6-android-jni`, `calaos-board`, `calaos-android-review`. Tous les tickets sont séquentiels (mêmes fichiers `android/`).

| ID | Titre | Taille | Bloqué par | Groupe | Statut |
|---|---|---|---|---|---|
| [AND-1](tasks/AND-1-toolchain-cmake.md) | Toolchain Android et configuration CMake | M | — | G-ANDROID | Terminé |
| [AND-2](tasks/AND-2-packaging-gradle-manifest.md) | Packaging Qt 6.11 : Gradle, manifest, ressources | M | AND-1 | G-ANDROID | Terminé |
| [AND-3](tasks/AND-3-glue-java-jni.md) | Glue Java/JNI au standard Qt 6 | M | AND-2 | G-ANDROID | Terminé |
| [AND-4](tasks/AND-4-push-fcm-java.md) | Notifications push FCM via le SDK Firebase Java | L | AND-3 | G-ANDROID | Terminé |
| [AND-7](tasks/AND-7-push-image-camera.md) | Image de caméra dans les notifications push | L | AND-4 | G-ANDROID | En revue |
| [AND-5](tasks/AND-5-release-aab-signature.md) | Build release : AAB multi-ABI et signature | S | AND-4 | G-ANDROID | Terminé |
| [AND-6](tasks/AND-6-doc-nettoyage.md) | Documentation Android et nettoyage legacy | S | AND-5, AND-7 | G-ANDROID + G-DOC | À faire |
| [AND-8](tasks/AND-8-clavier-apres-login.md) | Le clavier reste ouvert après le login | S | — | G-QML-MOBILE | Terminé |

Suivis issus des revues (à traiter dans le ticket indiqué) :
- AND-5 : `android/build.gradle` `useLegacyPackaging = legacyPackaging` reçoit la chaîne `"false"`, évaluée à vrai par Groovy → les `.so` sont compressés dans l'APK (`extractNativeLibs=true`). Trancher (`.toBoolean()` ou compression assumée) avant la release ; la permission `WRITE_EXTERNAL_STORAGE` insérée par Qt est inutile (`tools:node="remove"` ou `maxSdkVersion`).
- AND-5 : configurer avec le seul ABI `x86_64` s'arrête sur une erreur CMake non analysée (constaté pendant AND-1).
- AND-5 : si la minification R8 est activée, ajouter une règle keep pour les helpers Java appelés par nom depuis le JNI (`HardwareUtils.showAlertMessage`, `getNetworkStatus`, `inputtextDialog`, classe `HardwareUtilsNatives`).
- Nouveau ticket candidat : Android n'émet jamais `networkStatusChanged` (état réseau lu seulement au login) → `QNetworkInformation` ou `NetworkCallback`. Libellés « Confirm »/« Cancel » du dialogue de saisie non traduits.
- AND-9 : depuis QT-1, `enableOnBackInvokedCallback="true"` (modèle Qt 6.12) et Qt 6.12 appelle `moveTaskToBack()` quand le retour n'est pas accepté ; or `qml/mobile/main.qml:94-97` accepte toujours `Key_Back`, même à la racine → l'app ne passe jamais en arrière-plan. Le correctif AND-9 consiste à ne plus accepter `Key_Back` à la racine.
- Suivi FIX-3 : un tap mémorisé survit à une déconnexion volontaire et peut s'ouvrir sur une autre maison (besoin d'un chemin « logout utilisateur » distinct du logout iOS sur resign).
- Suivis QT-1 : passer `targetSdk` à 37 (revue des changements Android 17) ; Reachability iOS → `NWPathMonitor` ; ticket de nettoyage qmllint pour les nouvelles catégories 6.12 (`id-shadows-member`, `unqualified`) ; l'APK Android embarque les plugins QML desktop (androiddeployqt scanne tout le repo) ; tests unitaires non compilables avec `~/Qt/6.11.0/macos` + Xcode 27 (`__yield`), à revérifier avec 6.12.
- AND-7 : le changement serveur (messages FCM data seuls) est confié le 2026-10-16 à la session Happier « Calaos: Climate devices » (calaos_base). Contrat proposé : l'app envoie `"push_format": "data"` dans `register_push` (WS et HTTP), le serveur réserve le data seul (+ `android.priority=high`) aux jetons enregistrés ainsi ; l'app Play Store actuelle (Qt 5, Firebase C++) garde le format notification+data. AND-7 doit implémenter le contrat final documenté côté calaos_base.
- PR GitHub #31 (gwarf, 2026-10-16) « android: fix the Qt 6 build and launch crash » recoupe AND-1/AND-2 (Qt 6.10, Firebase C++ conservé) ; PR #18 (2021) « image preview in notification for Android » recoupe AND-7. À traiter avec Raoul avant de pousser `feat/android-qt6`.
- Suivis FIX-4 : sur un passage Wi-Fi → 4G l'app reste « connectée » alors que le websocket/long-poll de l'ancien réseau est mort (attente du timeout ping/poll ; iOS identique) → forcer une reconnexion sur changement de médium ; Android n'émet jamais `applicationWillResignActive` (session gardée en arrière-plan, reconnexion en arrière-plan) ; une app démarrée hors ligne ne se connecte pas automatiquement au retour du réseau (iOS identique).
- Suivi FIX-5 : `lang/calaos_de.ts` a 51 chaînes non traduites et 2 entrées obsolètes (constat lupdate 2026-10-16) ; lancer lupdate via `lang/lang.pro` (un arbre CMake iOS ou Android ne voit que ses propres sources et supprimerait les entrées de l'autre plateforme) ; les `.qm` embarqués via `lang.qrc` sont versionnés et doivent être régénérés avec les `.ts`.
- Suivis MOB-1 : (a) `LoginView` trop haute en paysage téléphone (logo coupé, bouton Login sous la barre gestuelle) → Flickable ou mise en page compacte ; (b) iPad en mode fenêtré (iPadOS 26+) : les contrôles de fenêtre en haut à gauche peuvent chevaucher l'icône d'en-tête (zone d'adaptation des coins non couverte par `safeAreaInsets`), à vérifier sur un vrai iPad ; (c) Android paysage : bande #080808 côté encoche et séparateur d'en-tête interrompu (letterbox unilatéral) → laisser fond et séparateur passer sous les marges latérales ; (d) Android 15+ en navigation 3 boutons : vérifier le voile de la barre de navigation au-dessus de la barre de menu. Preuves manquantes pour MOB-1 (passage appareils) : iPad paysage, pièce et scénarios peuplés, non-régression Android 12.
- FIX-7 (correction du contexte du ticket) : la cause annoncée (`ReconnectPolicy::suspend()` qui réarme après `stop()`) était fausse ; le cas Android ne se reproduit pas sur le code de base (probablement un démarrage à froid après logout, qui se reconnecte par conception). Les vrais chemins corrigés : iOS (logout → arrière-plan → premier plan reconnectait) et découverte desktop (latent). iOS met désormais la session en pause en arrière-plan au lieu d'un logout.
- Suivis FIX-8 : sur iOS, le retour au premier plan se connecte avec les propriétés `calaosApp` (`Application.cpp:91`), pas avec des valeurs tapées mais non validées dans l'écran de login ; `qml/mobile/main.qml:124-126` (`username: calaosApp.username` etc.) est du code mort depuis `LoginView.syncFromApp()` — à retirer après AND-9.
- Suivis FIX-9 (Raoul a validé les deux le 2026-10-16 → FIX-10 et FIX-11) : (a) l'alerte « No network » reste affichée au-dessus de l'accueil après la connexion automatique → la fermer sur `homeLoaded` ou la remplacer par un bandeau « hors ligne » non modal (préféré par la revue) ; (b) la déconnexion n'est pas mémorisée entre deux lancements : après « Se déconnecter », un relancement à froid se reconnecte automatiquement (en ligne comme hors ligne, comportement ancien) → drapeau « déconnecté » persistant lu par l'auto-login de lancement (Android : réutiliser `CredentialStore.isSessionActive` d'AND-7 ; iOS à créer). Le critère FIX-9 « logout puis démarrage hors ligne ⇒ aucune » n'est vrai qu'au sein d'un même processus.
- Constaté pendant la revue AND-9 (Qt 6.12, Android 16, chemin « key event » uniquement : `adb input keyevent BACK` / touche retour matérielle) : Qt appelle parfois `moveTaskToBack()` alors que le QML a accepté le relâchement ; déjà présent avant AND-9 ; un second retour a une fois terminé l'activité avec un SIGABRT FORTIFY (`hwuiTask0`) à la sortie. Geste et barre 3 boutons non affectés. Candidat à un rapport de bug Qt.
- Test externe (gwarf, PR #32, 2026-10-16) sur `6bc67db` : Pixel 11 Pro XL, Android 16, build Linux via `tools/build_android.sh Debug` (Qt 6.12.0, NDK 27.2, JDK 17) — démarrage, connexion à un vrai serveur, commande des lumières OK ; push : `POST_NOTIFICATIONS` accordée, canal `calaos_events` créé, jeton FCM obtenu et transmis au C++, `register_push` envoyé après login. Réception réelle d'une notification non vérifiée. Valide AND-2/AND-3/AND-4 (partie jeton/permission) et QT-1 sur un vrai appareil Android 16, et le script de build sous Linux.
- Passage appareil Redmi 9 (Android 12, 2026-10-16, build `6a3bfd4`) : démarrage, connexion wss, migration des identifiants vers le Keystore, jeton push, reconnexion automatique après coupure, lancement hors ligne puis connexion, push reçus au premier plan (image de caméra ajoutée) et en arrière-plan, tap → événement, bouton retour, dialogue en français ; 0 crash, aucune erreur JNI, aucun avertissement FIX-1/FIX-2. Non vérifiés sur appareil (laissés « En revue ») : FIX-8 (identifiants tapés) et FIX-11 (déconnexion après relance), FIX-3 (tap pendant une reconnexion), FIX-7/FIX-9 côté iOS, IOS-1/MOB-1/QT-1 sur iPhone, AND-9 en armv7, AND-7 avec le futur format data seul (changement calaos_base).
- AND-6 : `mobile.pro` liste encore en `DISTFILES` le wrapper Gradle et `libs.xml` supprimés ; `tools/check_sources.sh` ne reconnaît pas les chemins `$$PWD/src/...` de `calaos.pri` (faux positifs).
- QML (hors P-AND) : `main.qml:128` paramètre `idx` non déclaré ; `AnimatedIcon.qml:31` image `icon_light_00-1.png` introuvable.

## P-DEV — Derniers appareils et dernière version de Qt

Demandé par Raoul le 2026-10-16 : support des derniers iPhone (iOS 27, iPhone 17/18, Air), des dernières versions d'Android (15/16) et des builds multi-architecture, migration vers la version de Qt la plus récente (6.12.0). Outils de test : simulateurs iOS 27 (Xcode 27.1), émulateur `Calaos_API36_arm64` (Android 16), iPhone 15 Pro et Redmi 9 (Android 12) physiques. Skill iOS : `.claude/skills/qt6-ios-build`.

Ordre : AND-4 → QT-1 → (IOS-1 ∥ AND-7) → MOB-1 → AND-5 → AND-9 → AND-6.

| ID | Titre | Taille | Bloqué par | Groupe | Statut |
|---|---|---|---|---|---|
| [QT-1](tasks/QT-1-upgrade-qt-6-12.md) | Migration Qt 6.11.0 → Qt 6.12.0 (iOS + Android) | M | AND-4 | G-ANDROID + G-IOS + G-BUILD | En revue |
| [IOS-1](tasks/IOS-1-derniers-iphone.md) | Support des derniers iPhone (iOS 27, iPhone 17/18, Air) | M | QT-1 | G-IOS | En revue |
| [MOB-1](tasks/MOB-1-zones-securite.md) | Zones de sécurité : Dynamic Island iOS et edge-to-edge Android 15+ | M | IOS-1, AND-4 | G-QML-MOBILE | En revue |
| [AND-9](tasks/AND-9-android-15-16-multiarch.md) | Android 15/16 et matrice multi-architecture | M | AND-5, MOB-1 | G-ANDROID | En revue |

## P-FIX — Petits bugs constatés pendant P-AND

Bugs relevés dans les logs et les revues des tickets P-AND (2026-10-16). Non retenu : l'avertissement `QFAppListener::onMessageReceived` connecté par `SLOT()` vient de `3rd_party/quickflux` (non modifiable, sans effet).

Exécution parallèle : chaque ticket lancé en parallèle travaille dans son propre worktree git (`../calaos_mobile-wt/<ID>`, branche `wt/<ID>`), l'orchestrateur fusionne dans `feat/android-qt6` et tient ce board.

| ID | Titre | Taille | Bloqué par | Groupe | Statut |
|---|---|---|---|---|---|
| [FIX-1](tasks/FIX-1-avertissements-qml.md) | Avertissements QML à l'exécution (paramètres injectés, image d'animation) | S | — | G-QML-MOBILE + G-QML-SHARED | Terminé |
| [FIX-2](tasks/FIX-2-pusheventview-busyindicator.md) | PushEventView : ReferenceError `control` dans le BusyIndicator | S | — | G-QML-MOBILE | Terminé |
| [FIX-3](tasks/FIX-3-push-token-partage.md) | Push : tap hors connexion, rotation du jeton, jeton dans les logs | M | AND-4 | G-CONN + G-ANDROID | En revue |
| [FIX-4](tasks/FIX-4-reseau-android-changements.md) | Android : détecter les changements de réseau en cours d'exécution | S | FIX-3 | G-ANDROID | Terminé |
| [FIX-5](tasks/FIX-5-dialogue-android-i18n.md) | Android : libellés traduits des dialogues natifs | S | FIX-4 | G-ANDROID | Terminé |
| [FIX-6](tasks/FIX-6-check-sources-pwd.md) | tools/check_sources.sh : faux positifs sur les chemins `$$PWD/src/...` | S | — | G-BUILD | Terminé |
| [FIX-7](tasks/FIX-7-reconnexion-apres-logout.md) | Pas de reconnexion automatique après une déconnexion volontaire | S | AND-7 | G-CONN | En revue |
| [FIX-8](tasks/FIX-8-identifiants-tapes-ecrases.md) | Un changement de réseau écrase les identifiants tapés sur l'écran de login | S | FIX-7 | G-CONN | En revue |
| [FIX-9](tasks/FIX-9-demarrage-hors-ligne.md) | App lancée hors ligne : se connecter quand le réseau revient | S | FIX-8 | G-CONN | En revue |
| [FIX-10](tasks/FIX-10-bandeau-hors-ligne.md) | Bandeau « hors ligne » à la place de l'alerte modale « No network » | S | FIX-11, AND-9 | G-QML-MOBILE + G-CONN | En cours |
| [FIX-11](tasks/FIX-11-deconnexion-persistante.md) | Mémoriser la déconnexion volontaire entre deux lancements | S | FIX-9 | G-CONN + G-ANDROID | En revue |

## Diagramme de parallélisation

Chaque colonne est une lane exécutable par un agent indépendant ; `→` = séquence obligatoire, `‖` = parallèle.

```
P0   T01 → T02 → T03                          (lane unique G-BUILD)

P1   G-MODELS          G-CONN    G-QML        MISC
     T04 ‖ T05 ‖ T06    T08       T07 ‖ T10    T09

P2   G-MODELS          G-CONN(+APP)
     T11 → T15          T13 → T14              (2 lanes en parallèle)

P3   G-MODELS/COMMON      G-CONN     MISC
     T16 ────────┐        T17 ──┐    T19 ‖ T20
                 └── T18 ←──────┘
     puis sweeps sérialisés : T21 → T22

P4   G-QML-SHARED      G-QML-DESKTOP
     T23 → T24          T25 ‖ T26
     puis sweeps sérialisés : T27 → T28

P5   T29 → T30                                (T28 abandonné)
```

**Barrières de synchronisation** : fin de P0 (CI verte requise) · T14 avant T17 · T16+T17+T18 avant T21 · tous les tickets QML ciblés avant T27.

## Tickets à risque — vérification manuelle obligatoire (desktop ET mobile)

| Ticket | Risque | Vérification |
|---|---|---|
| **T14 Backoff** | Délais de reconnexion modifiés ; suppression du logout sur erreur transitoire | Coupure serveur 2 min → séquence 1/2/4/8…30 s sans chevauchement, un seul login à la reprise ; mode avion mobile ; suspend/resume desktop ; mauvais mot de passe → pas de retry infini. |
| **T13 Timeouts** | Un long-poll légitime pourrait être tué si mal calibré | 30 min sur serveur calme : aucun cycle logout/login parasite. |
| **T15 Caméras** | Cadence de rafraîchissement modifiée | Multi-caméras 10 min + navigation rapide : débit stable, pas de gel. |
| **T20 qqmlhelpers** | Une vue dépendant des notifications redondantes ne se rafraîchirait plus | Parcours complet : rooms, favoris, **audio (position de lecture)**, caméras, event log. |
| **T23/T27/T28 (visuels)** | Changements de pixels potentiels partout | Campagne de screenshots avant/après sur les vues principales des deux variantes. |

## Hors périmètre — backlog futur

Explicitement exclus de ce backlog, à traiter dans une phase dédiée :

- **Sweep couleurs : tokens Theme partout** (ex-T28, spec conservée dans `docs/tasks/T28-sweep-theme-couleurs.md`) — écarté par Raoul le 2026-08-11. 328 couleurs en dur et une dérive de casse `#3ab4d7` / `#3AB4D7`, mais c'est le plus gros sweep du backlog, il figure dans la table des risques avec campagne de screenshots obligatoire, et il ne corrige aucun défaut. T29 et T30 n'en dépendaient pas sur le fond : ils traitent les imports et les logs, ils n'étaient sérialisés derrière lui que par le groupe G-QML-SWEEP. T29 passe derrière T27, T30 derrière T29.
- **TLS TOFU / pinning d'empreinte certificat** (ex-T12, spec conservée dans `docs/tasks/T12-tls-tofu.md`) — écarté par Raoul le 2026-08-10 : verrouiller le certificat est trop risqué pour le parc installé, un renouvellement côté serveur couperait les clients. `ignoreSslErrors` reste donc en place. T13 et T14 n'en dépendaient pas sur le fond : ils n'étaient sérialisés derrière lui que parce que les trois touchent `CalaosConnection.cpp` (groupe G-CONN). T13 est repositionné derrière T08.
- **Migration CMake** (qmake est déprécié pour Qt6) — préalable recommandé au split transport complet de CalaosConnection.
- **Android** : `targetSdkVersion 29` (bloque les mises à jour Play Store, minimum requis 34), **OpenSSL 1.0.2r embarqué (EOL 2019, critique)**, toolchain Gradle 2021 + `jcenter()` mort, credentials en clair dans QSettings (chantier Keystore).
- **iOS** : APIs UIKit dépréciées (`UIAlertView`…), `KeychainItemWrapper` mort à supprimer, étapes Xcode manuelles non scriptables.
- **Docker** : image de base non pinnée, fetch AUR sans checksum.
- **Version applicative** : `src/version.h` codé en dur à `"git"` — injecter la vraie version au build.
- **i18n** : catalogues .ts périmés, `auto_translate.go` cassé par défaut, langues it/es esquissées mais absentes.
- Split transport WS/HTTP complet de CalaosConnection (T17 capture l'essentiel de la valeur testabilité).
