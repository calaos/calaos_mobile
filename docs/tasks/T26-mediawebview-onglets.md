# T26 — MediaWebView : source de vérité unique des onglets + correctifs navigateur

| Champ | Valeur |
|---|---|
| Phase | P4 — Consolidation QML |
| Taille | M |
| Bloqué par | T03 |
| Groupe de conflit | G-QML-DESKTOP (fichiers disjoints de T25 → parallélisable avec T25) |
| Variantes | desktop uniquement (nécessite un build avec webenginequick) |

## Contexte

Le navigateur web desktop (feature récente) a quatre défauts :

1. **Double source de vérité** : `qml/desktop/MediaWebView.qml:337-411` maintient un tableau JS `tabWebViews` (ligne 16) manuellement synchronisé avec un `ListModel tabListModel` (ligne 19), indexés par le même entier. Chaque mutation touche les deux (`push`+`append` 347-348, `splice`+`remove` 375-378) ; le fixup d'index de `closeTab` (383-392) est alambiqué. Toute dérive = onglets désynchronisés.
2. `MediaWebView.qml:341` : le résultat de `webViewComponent.createObject(...)` est utilisé **sans null-check** (la création peut échouer).
3. `MediaWebView.qml:106-112` : la recherche construit `"https://google.com/search?q=" + text` **sans URL-encoding** (espaces/`&`/`#` cassent l'URL) et le moteur est codé en dur.
4. `maxTabs: 10` est déclaré indépendamment dans `MediaWebView.qml:17` **et** `DialogTabList.qml:11`.

## Fichiers

- `qml/desktop/MediaWebView.qml`
- `qml/desktop/DialogTabList.qml`
- `qml/desktop/WebBrowserMenu.qml`, `qml/desktop/WebBrowserMenuItem.qml` (couleurs → Theme, opportuniste car mêmes fichiers)

## Implémentation

1. **Une seule structure** : stocker la référence de WebView directement dans le rôle du `ListModel` (`tabListModel.append({title, url, webView: obj})`) ou utiliser un `ObjectModel`. Supprimer `tabWebViews` et tous les fixups d'index doubles. `closeTab`/`switchToTab` opèrent sur la structure unique.
2. Null-check du `createObject` : si null, `console.warn` + ne pas créer l'onglet.
3. Recherche : `encodeURIComponent(text)` ; extraire l'URL du moteur en `property string searchEngineUrl` (défaut Google, remplaçable).
4. `maxTabs` : une seule déclaration dans MediaWebView, exposée à `DialogTabList` via property (le dialog est instancié depuis MediaWebView — vérifier le lien d'instanciation).
5. Couleurs en dur des fichiers touchés → tokens `Theme` existants (`Theme.blueColor`, etc.).

## Critères d'acceptation

- Ouvrir 10 onglets, en fermer au milieu, basculer entre eux, fermer l'onglet courant : titres/contenus toujours synchronisés (plus de désynchronisation possible par construction).
- Recherche avec `c++ & qt #test` → URL correctement encodée.
- La limite d'onglets est cohérente entre la toolbar et le dialog.
- qmllint OK ; build desktop avec `HAVE_WEBENGINE`.

## Vérification

Session desktop avec webengine : scénario onglets complet + recherches avec caractères spéciaux + bookmarks (non touchés mais vérifier la non-régression du dialog).
