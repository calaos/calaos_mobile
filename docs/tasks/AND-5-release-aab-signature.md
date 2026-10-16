# AND-5 — Build release : AAB multi-ABI et signature

| Champ | Valeur |
|---|---|
| Phase | P-AND — Android Qt 6 |
| Taille | S |
| Bloqué par | AND-4 |
| Groupe de conflit | G-ANDROID |
| Variantes | mobile (Android) |

## Contexte

Le Play Store exige un AAB signé, les 4 ABI, target SDK 36 et l'alignement 16 KB. Le keystore d'upload Calaos n'est pas dans le repo et ne doit pas y entrer.

## Fichiers

- `tools/build_android.sh`, `cmake/Android.cmake`, `android/build.gradle` si nécessaire

## Implémentation

1. `tools/build_android.sh Release [--aab]` : 4 ABI (`armeabi-v7a;arm64-v8a;x86;x86_64`) via les kits Qt installés, cible `aab`.
2. Signature lue depuis des variables d'environnement (`CALAOS_KEYSTORE`, `CALAOS_KEYSTORE_PASS`, `CALAOS_KEY_ALIAS`, `CALAOS_KEY_PASS`) ; sans elles, build non signé avec un avertissement clair.
3. `versionName` / `versionCode` issus de `VERSION.txt` (déjà en place via `cmake/Versioning.cmake`) — vérifier qu'ils arrivent dans le manifest final.

## Critères d'acceptation

- `tools/build_android.sh Release --aab` produit un `.aab` contenant les 4 ABI (`unzip -l` sur `base/lib/`).
- Avec un keystore de test, l'AAB est signé (`jarsigner -verify`).
- `versionCode` / `versionName` corrects dans le manifest de l'AAB (`bundletool dump manifest` ou équivalent).
- Alignement 16 KB vérifié sur les `.so` des 4 ABI.
- Aucun secret dans le repo ni dans les logs du script.
