# T05 — EventLogModel : résolution des IO output cassée

| Champ | Valeur |
|---|---|
| Phase | P1 — Bugs critiques |
| Taille | S |
| Bloqué par | T02 |
| Groupe de conflit | G-MODELS |
| Variantes | desktop + mobile |

## Contexte

Dans `EventLogItem::load` (`src/EventLogModel.cpp:141-144`) :

```cpp
IOBase *io = IOCache::Instance().searchInput(data["io_id"].toString());
if (!io)
    IOCache::Instance().searchOutput(data["io_id"].toString());  // résultat jeté !
if (io)
```

Le résultat de `searchOutput()` n'est **jamais réaffecté à `io`**. Conséquence : les entrées du journal d'événements concernant des IO *output* (lumières, prises, volets…) ne résolvent jamais leur nom, leur pièce ni leur icône — elles s'affichent anonymes. Le pattern correct existe dans `FavoritesModel.cpp:70-71`.

## Fichiers

- `src/EventLogModel.cpp` (ligne 143)
- **Nouveau** : `tests/tst_eventlogmodel/` (+ ajout au `SUBDIRS` de `tests/tests.pro`)

## Implémentation

1. Ligne 143 : `io = IOCache::Instance().searchOutput(data["io_id"].toString());`
2. Test : peupler `IOCache` avec un IO présent **uniquement en output**, construire un `EventLogItem` et charger un JSON d'événement (`io_id` correspondant), vérifier que `evTitle` == nom de l'IO. Ajouter le cas symétrique (IO en input) et le cas absent (pas de crash, titre par défaut).

## Critères d'acceptation

- Test unitaire vert sur les trois cas (output, input, inconnu).
- `make check` global vert.

## Vérification

Manuel : sur mobile, ouvrir l'Event Log après avoir allumé/éteint une lumière — l'entrée affiche le nom de la lumière et sa pièce au lieu d'une entrée anonyme.
