# T21 — Constantes de clés JSON (sweep src/, sérialisé)

| Champ | Valeur |
|---|---|
| Phase | P3 — Découplage C++ |
| Taille | M |
| Bloqué par | T16, T17, T18 (touche tout src/ — doit passer après les refactorings) |
| Groupe de conflit | sweep src-wide — **sérialisé avec T22**, aucun autre ticket C++ en parallèle |
| Variantes | desktop + mobile |

## Contexte

Les clés du protocole JSON (`"cn_user"`, `"cn_pass"`, `"action"`, `"state"`, `"id"`, `"gui_type"`, `"value"`, `"msg"`, `"data"`, `"type_str"`, `"player_id"`, …) sont des littéraux répétés dans `CalaosConnection.cpp`, `RoomModel.cpp`, `AudioModel.cpp`, `EventLogModel.cpp`, `Application.cpp`, `CameraModel.cpp`, `FavoritesModel.cpp`. Une faute de frappe ou un renommage côté serveur casse silencieusement (la clé absente donne une valeur par défaut).

## Fichiers

- **Nouveau** : `src/JsonKeys.h` (header-only) → **à enregistrer dans `calaos.pri` (HEADERS)**
- Application dans : `src/CalaosConnection.cpp`, `src/CalaosEventDecoder.cpp` (créé par T17), `src/RoomModel.cpp`, `src/EventLogModel.cpp`, `src/AudioModel.cpp`, `src/CameraModel.cpp`, `src/FavoritesModel.cpp`, `src/Application.cpp`

## Implémentation

1. Inventaire préalable : grep des littéraux entre crochets `["..."]` et des `insert("...")` sur les QJson*/QVariantMap dans src/ ; lister les clés protocole (exclure les clés de QSettings et autres usages internes).
2. `src/JsonKeys.h` : namespace de constantes, ex. :
   ```cpp
   namespace JsonKeys {
   inline constexpr QLatin1StringView CnUser{"cn_user"};
   inline constexpr QLatin1StringView State{"state"};
   // ...
   }
   ```
3. Remplacement **purement mécanique** — une clé = une constante ; **aucun renommage de clé réseau**, aucune autre modification.

## Critères d'acceptation

- Grep des littéraux remplacés dans src/*.cpp = 0 occurrence hors `JsonKeys.h` (liste des clés couvertes jointe à la PR).
- `make check` vert (les tests P0-P3, notamment tst_eventdecoder, garantissent l'équivalence).
- Build desktop + mobile ; session manuelle rapide (login + rooms + audio).

## Vérification

`make check` + login/session courte contre demo.calaos.fr.
