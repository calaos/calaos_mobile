# T09 — WeatherInfo : fuite mémoire dans forecastDataClear

| Champ | Valeur |
|---|---|
| Phase | P1 — Bugs critiques |
| Taille | S |
| Bloqué par | — |
| Groupe de conflit | indépendant (personne d'autre ne touche WeatherInfo avant T22) |
| Variantes | desktop uniquement |

## Contexte

`WeatherModel::forecastDataClear()` (`src/WeatherInfo.cpp:228-231`) est le callback de clear de la `QQmlListProperty` des prévisions :

```cpp
void WeatherModel::forecastDataClear() { dataForecast.clear(); }
```

`dataForecast` possède des `WeatherData*` bruts : le `clear()` les abandonne **sans `delete`** → fuite à chaque rafraîchissement météo. Le pattern correct existe déjà dans le même fichier : boucle de delete lignes ~119-121 et destructeur lignes ~48-50.

## Fichiers

- `src/WeatherInfo.cpp` (lignes 228-231)

## Implémentation

`qDeleteAll(dataForecast); dataForecast.clear();` — aligné sur le pattern du destructeur. Vérifier qu'aucun autre code ne conserve de pointeur vers ces objets après le clear (grep des usages de `dataForecast`).

## Critères d'acceptation

- Build desktop OK.
- Pas de croissance mémoire sur rafraîchissements météo répétés (observation `top`/`valgrind` sur quelques cycles, ou relecture simple vu la taille du fix).

## Vérification

Build desktop + laisser tourner le widget météo sur plusieurs cycles de rafraîchissement.
