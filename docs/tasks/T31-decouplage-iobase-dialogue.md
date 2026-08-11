# T31 — Découpler IOBase → couche dialogue (askStateText)

| Champ | Valeur |
|---|---|
| Phase | P6 — Suite du découplage |
| Taille | S |
| Bloqué par | T18 |
| Groupe de conflit | G-MODELS |
| Variantes | desktop + mobile |

## Contexte

`IOBase::askStateText()` (`src/RoomModel.cpp`) ouvre elle-même une boîte de saisie
pour demander une nouvelle valeur à l'utilisateur, par deux chemins selon la
variante :

```cpp
#if defined(CALAOS_MOBILE)
    connect(HardwareUtils::Instance(), &HardwareUtils::dialogTextValid, ...);
    HardwareUtils::Instance()->inputTextDialog(tr("Change value"), tr("Enter new value"));
#else
    QFAppDispatcher *appDispatcher = QFAppDispatcher::instance(engine);
    appDispatcher->dispatch("openAskTextForIo", m);
#endif
```

`IOBase` est une classe de modèle : elle représente un point d'entrée/sortie de
la maison. Qu'elle sache qu'une boîte de dialogue existe est le même couplage
inversé que celui traité en T18 entre `RoomModel` et le transport, cette fois
vers la couche interface.

**La conséquence est mesurable, pas théorique.** `RoomModel.cpp` dépend donc en
dur de `HardwareUtils` (branche mobile) *et* de quickflux (branche desktop), et
ce n'est pas éliminable au lien : `<qfappdispatcher.h>` est inclus sans garde et
`askStateText` est `Q_INVOKABLE`, donc moc génère une entrée de table de
dispatch qui la référence. L'éditeur de liens ne peut pas la supprimer même
quand la branche ne sert pas.

Trois tests le paient aujourd'hui :

- `tests/tst_roomfiltermodel/` ne peut pas utiliser le vrai `RoomModel` et lie
  une doublure `roommodel_stub.cpp` ;
- `tests/tst_homemodel/` et `tests/tst_favoritesmodel/` lient `HardwareUtils` et
  `QT += widgets`, plus un `INCLUDEPATH` vers quickflux, pour tester
  respectivement un `clear()` et une sérialisation de favoris.

T18 a retiré la dépendance au transport ; celle-ci est la dernière qui empêche
de tester un modèle sans monter la moitié de l'application.

## Fichiers

- `src/RoomModel.cpp` / `src/RoomModel.h` (`IOBase::askStateText`, `IOBase::textDialogValid`)
- `qml/desktop/main.qml` et `qml/mobile/main.qml` (le branchement plateforme y remonte)
- `tests/tst_roomfiltermodel/`, `tests/tst_homemodel/`, `tests/tst_favoritesmodel/` (allègement des `.pro`)

## Implémentation

Option privilégiée : `IOBase` **émet un signal** (`textInputRequested(IOBase *io)`
ou équivalent) et ne connaît plus ni `HardwareUtils` ni le dispatcher. La couche
qui sait comment demander une valeur à l'utilisateur — le QML de chaque variante,
via `Application` ou le modèle racine — s'y abonne et rappelle
`IOBase::textDialogValid(text)` quand l'utilisateur a validé.

Le `#if defined(CALAOS_MOBILE)` disparaît alors du modèle : desktop et mobile
choisissent chacun leur présentation dans leur propre `main.qml`, ce qui est
déjà le cas pour le reste des dialogues (cf. le filtre `openAskTextForIo` de
`qml/desktop/main.qml`).

Attention : `askStateText` est `Q_INVOKABLE` et appelée depuis le QML
(`IOVarString`, `IOVarInt`…). Sa signature publique doit rester utilisable
telle quelle, ou tous ses appelants QML doivent être mis à jour dans le même
ticket.

## Critères d'acceptation

- `grep -n "HardwareUtils\|qfappdispatcher" src/RoomModel.cpp` = 0.
- Plus aucun `#ifdef CALAOS_MOBILE` dans `IOBase`.
- **Au moins un des trois tests cités allège réellement son `.pro`** : c'est la
  mesure du ticket. Si aucun n'y arrive, le découplage est cosmétique et il faut
  le dire plutôt que le livrer.
- `make check` vert, builds desktop et mobile verts.

## Vérification

Session manuelle sur les deux variantes : sur un IO de type `var_string` et un
`var_int`, déclencher la saisie depuis la vue pièce, valider une valeur, vérifier
qu'elle part au serveur ; puis annuler la saisie et vérifier qu'aucune valeur
n'est envoyée. Le chemin mobile utilise un dialogue natif, le desktop le clavier
virtuel : les deux doivent se comporter comme avant.
