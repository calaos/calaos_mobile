# T15 — Polling caméra maîtrisé + image providers thread-safe

| Champ | Valeur |
|---|---|
| Phase | P2 — Robustesse réseau |
| Taille | M |
| Bloqué par | T11 (mêmes fichiers) |
| Groupe de conflit | G-MODELS |
| Variantes | desktop + mobile |

## Contexte

1. **Multiplication des requêtes caméra** : `CameraItem::cameraPictureDownloaded` (`src/CameraModel.cpp:199`) et `cameraPictureFailed` (215) ré-arment chacun un `QTimer::singleShot(200ms)` qui rappelle `getCameraPicture` tant que `cameraVisible`. Chaque passage de `cameraVisibleChanged` à true (`CameraModel.cpp:100-104`, et 29-30) démarre une **nouvelle chaîne de polling indépendante** ; rien n'arrête les chaînes existantes → basculer la visibilité multiplie le débit de requêtes.
2. **Course de threads sur les images** : `AudioImageProvider` et `CameraImageProvider` sont des `QQuickImageProvider::Image` (`AudioModel.cpp:302-304`, `CameraModel.cpp:327-329`) dont `requestImage` peut s'exécuter **hors du thread GUI**. Or ils scannent linéairement le modèle (`model->rowCount()/item(i)`) et lisent `currentImage`/`currentCoverImage` que le thread GUI mute — accès croisé non synchronisé.
3. **Duplication** : les deux `requestImage` (`AudioModel.cpp:67-102` et `CameraModel.cpp:144-179`) sont quasi identiques (split de l'id, garde `toInt()<0`, scan linéaire, scale).
4. Mineur : `AudioPlayer::startPolling` (`AudioModel.cpp:271`) fait `delete pollTimer` brut sur un QObject potentiellement actif → préférer `deleteLater` ou réutiliser le timer.

## Fichiers

- `src/CameraModel.cpp/.h`
- `src/AudioModel.cpp/.h`
- **Nouveau** : `src/ModelImageProvider.h`, `src/ModelImageProvider.cpp` → **à enregistrer dans `calaos.pri`**

## Implémentation

1. **Polling caméra** : remplacer les chaînes `singleShot` par un **QTimer membre par CameraItem** (200 ms, `setSingleShot(false)` ou re-arm explicite), démarré/arrêté par `cameraVisibleChanged`. Alternative acceptable : compteur de génération incrémenté à chaque changement de visibilité, les callbacks d'une génération périmée s'abandonnent. L'important : à tout instant, **au plus une** chaîne de polling par caméra visible, zéro pour une caméra invisible.
2. **`ModelImageProvider`** : classe de base factorisant les deux `requestImage` — le modèle (thread GUI) **pousse une copie** de chaque image dans un cache `QHash<QString, QImage>` protégé par `QMutex` ; `requestImage` (thread provider) ne fait que lire le cache sous mutex. Plus aucun accès au `QStandardItemModel` depuis le thread du provider. `AudioImageProvider`/`CameraImageProvider` deviennent des spécialisations minces.
3. `startPolling` audio : supprimer le `delete` brut (réutiliser le timer existant ou `deleteLater`).

## Critères d'acceptation

- Test manuel : basculer rapidement la visibilité de 2+ caméras pendant 1 min → le débit de requêtes reste constant (compter les lignes de log de requêtes caméra).
- Pas de crash ni tearing d'image en défilement rapide de la liste caméras.
- 10 min d'affichage multi-caméras continu : débit stable, pas de gel.
- Build desktop + mobile ; `make check` vert.

## Vérification

Vue multi-caméras sur serveur réel, navigation rapide entre pages/vues, observation des logs de requêtes.
