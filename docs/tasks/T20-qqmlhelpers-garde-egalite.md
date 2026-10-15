# T20 — qqmlhelpers : garde d'égalité dans les setters de propriétés modèle

| Champ | Valeur |
|---|---|
| Phase | P3 — Découplage C++ |
| Taille | S |
| Bloqué par | T16 (séquencement de prudence — techniquement indépendant) |
| Groupe de conflit | G-COMMON |
| Variantes | desktop + mobile |

## Contexte

Les macros `QML_READONLY_PROPERTY_MODEL` / `QML_WRITABLE_PROPERTY_MODEL` (`src/qqmlhelpers.h:68-103`) font `setData` + `emit …Changed` **à chaque écriture, même si la valeur est inchangée** — contrairement à `QML_WRITABLE_PROPERTY` (non-modèle) qui garde avec `if (m_##name != name)`. Conséquence : chaque refresh d'état (chaque poll serveur appelle les `update_*` d'`IOBase`) déclenche des `dataChanged` et réévaluations de bindings redondants sur toute l'UI.

## Fichiers

- `src/qqmlhelpers.h` (macros WRITE, lignes 68-103)

## Implémentation

Ajouter la garde d'égalité en tête des setters générés par les macros modèle : `if (m_##name == name) return;` (aligné sur la macro non-modèle). Attention aux types sans `operator==` trivial — vérifier les types réellement utilisés avec ces macros (grep des usages dans src/).

⚠️ Risque : si une vue dépendait des notifications redondantes pour se rafraîchir (mauvaise pratique mais possible), elle ne se met plus à jour. D'où la campagne de vérification ci-dessous.

## Critères d'acceptation

- Compile desktop + mobile ; `make check` vert.
- Instrumentation ponctuelle (compteur temporaire ou qDebug) : le nombre de signaux `xxxChanged` émis pendant 1 min de polling chute drastiquement.
- L'UI se met **toujours** à jour sur vrai changement d'état.

## Vérification (parcours complet des deux variantes)

Rooms (états d'IO en direct), favoris, **audio (position de lecture — elle change à chaque poll, elle doit continuer de défiler)**, caméras, event log. Toute vue qui ne se rafraîchit plus = régression à corriger avant merge.
