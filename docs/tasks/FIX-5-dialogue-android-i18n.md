# FIX-5 — Android : libellés traduits des dialogues natifs

| Champ | Valeur |
|---|---|
| Phase | P-FIX — Petits bugs constatés pendant P-AND |
| Taille | S |
| Bloqué par | FIX-4 |
| Groupe de conflit | G-ANDROID |
| Variantes | mobile (Android) |

## Contexte

Relevé pendant AND-3/AND-4 : les boutons « Confirm » / « Cancel » du dialogue de saisie natif (`HardwareUtils.java` `inputtextDialog`) sont en anglais en dur. Les ressources Android ne peuvent pas porter les traductions : le modèle Gradle de Qt fixe `resConfig "en"`, qui retire toutes les locales de l'APK. L'app est traduite en FR/DE via Qt Linguist (`lang/*.ts`).

## Fichiers

- `android/HardwareUtils_Android.cpp`, `android/src/fr/calaos/calaoshome/HardwareUtils.java`
- `lang/calaos_fr.ts`, `lang/calaos_de.ts`

## Implémentation

Passer les libellés traduits depuis le C++ (`tr()`) au Java en paramètres (dialogue de saisie et bouton de l'alerte si concerné), mettre à jour les `.ts` (lupdate) et fournir les traductions FR/DE.

## Critères d'acceptation

- Téléphone en français : boutons du dialogue de saisie en français ; en allemand : en allemand ; sinon anglais.
- Chaînes présentes dans `lang/calaos_fr.ts` et `calaos_de.ts`, traduites.
