# T19 — FavoritesModel : gérer les types de favoris non-FavIO (TODO!)

| Champ | Valeur |
|---|---|
| Phase | P3 — Découplage C++ |
| Taille | M |
| Bloqué par | — (fichier vierge de tout autre ticket) |
| Groupe de conflit | indépendant |
| Variantes | desktop + mobile |

## Contexte

Deux `qDebug() << "TODO!";` dans `src/FavoritesModel.cpp` :
- Ligne 44 (`save()`) : tout favori dont le type n'est pas `FavIO` est **silencieusement ignoré à la sauvegarde** — il disparaît au prochain redémarrage.
- Ligne 83 (`addFavorite`) : l'ajout d'un favori non-FavIO retourne false sans rien faire.

Les types concernés sont dans `Common::FavoriteType` (`src/Common.h:80-87`) : `FavLightsCount`, `FavShutterCount`, `FavAudio`, `FavCamera`, etc. Un consommateur existant : `qml/SharedComponents/IOFavAllLights.qml`.

## Fichiers

- `src/FavoritesModel.cpp` (lignes 44, 83 et la logique save/load associée)
- `src/FavoritesModel.h`

## Implémentation

1. Inventorier les `Common::FavoriteType` réellement atteignables depuis l'UI (grep des usages dans qml/ et src/).
2. Implémenter au minimum le **round-trip de persistance** pour tous les types : sérialiser type + données associées (id de caméra, player audio…) dans `save()`, les restituer au load. Si un type n'a pas encore d'UI d'ajout, il doit néanmoins être **préservé** (pas droppé) s'il est présent dans les données, avec un `qWarning` explicite au lieu du TODO.
3. Supprimer les deux `qDebug() << "TODO!"`.

## Critères d'acceptation

- Ajouter/sauver/recharger un favori de chaque type constructible depuis l'UI ne perd rien (test unitaire sur la sérialisation du modèle si l'instanciation est raisonnable, sinon vérification manuelle documentée dans la PR).
- Plus aucun `TODO!` dans FavoritesModel.cpp.
- `make check` vert ; build desktop + mobile.

## Vérification

Mobile : ajouter un favori de chaque type proposé par `FavoritesAddView`, tuer/relancer l'app, vérifier que tous sont restaurés.
