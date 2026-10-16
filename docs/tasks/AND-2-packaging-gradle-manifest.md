# AND-2 — Packaging Qt 6.11 : Gradle, manifest, ressources

| Champ | Valeur |
|---|---|
| Phase | P-AND — Android Qt 6 |
| Taille | M |
| Bloqué par | AND-1 |
| Groupe de conflit | G-ANDROID |
| Variantes | mobile (Android) |

## Contexte

`android/` est le `QT_ANDROID_PACKAGE_SOURCE_DIR` : chaque fichier qui s'y trouve **remplace** le modèle généré par Qt. Ils datent de Qt 5 :

- `android/build.gradle` : AGP 4.1.2, `jcenter()` (fermé), `compile` (supprimé de Gradle), pas de `namespace`, Java 1.8, `qt5AndroidDir`, application du SDK Firebase C++ ;
- `android/gradle/wrapper` : Gradle 6.8.3 ; `android/settings.gradle` : `includeBuild` du SDK Firebase C++ ;
- `android/res/values/libs.xml` : version Qt 5 (« ministro qt-5.14 ») qui casse le chargement des libs Qt 6 ;
- `android/openssl_prebuilt/` : OpenSSL 1.0 (EOL, ABI mips) — remplacé par KDAB depuis AND-1 ;
- `android/AndroidManifest.xml` : attribut `package=`, meta-data Qt 5, pas de min/target cohérents (Qt 6.11 : minSdk 28 ; Play Store : target 36).

Référence : modèles de `~/Qt/6.11.0/android_arm64_v8a/src/android/templates/` (AGP 9.0.0, Gradle 9.3.1, Java 17). Voir le skill `qt6-android-build`.

## Fichiers

- `android/build.gradle`, `android/settings.gradle`, `android/gradle.properties`, `android/project.properties`
- `android/AndroidManifest.xml`, `android/res/`
- Suppressions : `android/gradle/`, `android/gradlew*`, `android/res/values/libs.xml`, `android/openssl_prebuilt/`
- `cmake/Android.cmake` si nécessaire

## Implémentation

1. Réécrire `android/build.gradle` à partir du modèle Qt 6.11, en ne gardant que nos ajouts (pas encore de Firebase : il arrive en AND-4). `namespace "fr.calaos.calaoshome"`, `applicationId` idem.
2. Supprimer de `android/` tout ce que Qt génère lui-même (wrapper Gradle, `libs.xml`, propriétés obsolètes) ; `settings.gradle` minimal ou supprimé.
3. Manifest : repartir du modèle Qt 6.11, conserver l'activité `fr.calaos.calaoshome.HardwareUtils` (`singleTop`, exportée, launcher), le label et l'icône ; retirer `package=`, les meta-data Qt 5 et `requestLegacyExternalStorage`.
4. Supprimer `android/openssl_prebuilt/`.
5. Vérifier l'alignement 16 KB des `.so` et de l'APK (commandes dans le skill).

**Hors périmètre** : corrections Java/JNI (AND-3), push (AND-4), signature release (AND-5).

## Critères d'acceptation

- `tools/build_android.sh Debug` produit un APK debug.
- L'APK s'installe sur le téléphone de test (`adb install -r`), l'app démarre et affiche l'écran de login ; aucun crash dans `adb logcat -b crash`.
- Connexion TLS à un serveur Calaos (ou au mode démo `demo.calaos.fr`) fonctionnelle : OpenSSL 3 chargé (pas d'erreur `qt.network.ssl` dans logcat).
- `zipalign -c -P 16` passe sur l'APK.
- Plus aucune trace de Qt 5 dans `android/` (`qt5AndroidDir`, `libs.xml`, `jcenter`, wrapper Gradle).

## Vérification

Installer, lancer, se connecter en mode démo, naviguer dans 2–3 vues ; capturer les lignes clés de logcat.
