# Software Update UI — Suivi d'implémentation

> Feature : implémentation de la page de mise à jour logicielle (`qml/desktop/ConfigUpdateView.qml`)
> via l'API calaos-container. Plan détaillé : `~/.claude/plans/dans-ce-projet-dans-declarative-kay.md`

## Vague 1 — fondations (parallèle)

- [x] **T1 — Endpoint Go `GET /api/update/installed`** *(agent: sonnet)* ✓ gofmt clean, tests parseur 5/5 (module isolé) — `make test` complet à relancer dans le devcontainer (libapt-pkg-dev absent ici)
  `calaos-container/app/app.go`, `app/update.go`, `models/installed.go` + tests.
  Liste les paquets `calaos-*` installés (dpkg-query), ImageMap brute `{name:{name,version}}`.
- [x] **T2 — Transport C++** *(agent: sonnet)* ✓ syntax-check clean (-Wall -Wextra)
  `src/NetworkRequest.h/.cpp` (getHttpStatusCode), `src/CalaosOsAPI.h/.cpp` :
  `doGetRaw/doPostRaw` (endpoints update/* sans enveloppe) + 6 méthodes update + getters.
- [x] **T3 — Modèles + grouping** *(agent: sonnet)* ✓ syntax-check clean (-Wall -Wextra)
  `src/UpdateModel.h/.cpp` (UpdatePackagesModel + UpdateGroupsModel, pattern ControlPanelModel),
  `src/UpdateGrouping.h` (règles kernel/system), enregistrement `desktop.pro`.

## Vague 2 — orchestration

- [x] **R1 — Review T1** *(agent: sonnet)* ✓ APPROVE — sémantique dpkg-query et contrat JSON vérifiés en live, 2 nits (1 corrigé). Reste : `make test` dans le devcontainer avant push.
- [x] **T4 — UpdateManager** *(agent: fable)* ✓ syntax-check clean (desktop + mobile), POST-authoritative + resync + compat Qt 6.4
  `src/UpdateManager.h/.cpp` : state machine, WebSocket `/api/events` + fallback polling,
  boucle séquentielle groupe, timers (2 min + 12 h), dedup popup (hash), resync self-update,
  `finishUpgrade` (restart/reboot intelligent). Câblage `Application.h/.cpp` + `desktop.pro`.

## Vague 3 — C++ review puis UI (parallèle)

- [x] **R2 — Review cluster C++** *(agent: opus)* ✓ NEEDS-CHANGES : 1 blocker (resync dead-end),
  3 majors (adoption steady-state, m_upgradeSet, POST coupé par self-update daemon), 5 minors, 5 nits.
- [x] **T4b — Corrections findings R2** *(agent: fable)* ✓ 14/14 corrigés, 7 scénarios adverses validés, compile clean
- [x] **T5 — UI page** *(agent: fable + skill frontend-design)* ✓ qmllint 0 erreur, design system cohérent (status dots, panneau progression)
  `qml/desktop/ConfigUpdateView.qml` (sections Calaos/System, badges, boutons),
  `qml/desktop/UpdateProgressPanel.qml`, footer, dialog confirm, `qml_desktop.qrc`.
- [x] **T6 — Popup + actions** *(agent: sonnet)* ✓ qmllint OK, action/Filter/dispatch alignés
  `qml/desktop/DialogUpdateAvailable.qml`, `qml/quickflux/ActionTypes.qml`,
  `qml/desktop/main.qml` (Filter + instance), `qml_desktop.qrc`.

## Vague 4 — validation finale

- [x] **R3 — Review finale** *(agent: opus)* ✓ NEEDS-CHANGES : contrat QML↔C++ 100% propre (A1-A7 PASS) ;
  1 major (DialogReboot reboote au lieu de restart app), 5 minors, 9 nits.
- [x] **T7 — Corrections findings R3** *(agent: fable)* ✓ 11 fixes appliqués (DialogReboot, UpdateProgressModel, dismiss bannière, garde anti-stacking…), compile + qmllint iso-baseline

- [x] **T8 — Retrait du fallback dpkg-query** : calaos_home tourne dans un Docker sur Calaos OS,
  le dpkg de l'hôte est inaccessible — tout passe par l'API calaos-container. Si l'endpoint
  `/update/installed` échoue (404/injoignable), l'UI dégrade (`installedListAvailable=false`).

## ✅ Implémentation terminée (2026-08-05)

Reste à faire manuellement :
1. `cd calaos-container && make && make test` **dans le devcontainer** (libapt-pkg-dev requis), puis commit + push (⚠ auto-release).
2. Build desktop réel (CI/docker) — ici seule la vérif `g++ -fsyntax-only` était possible (core5compat absent).
3. `lupdate` pour régénérer `lang/calaos_fr.ts` / `calaos_de.ts` (nouvelles chaînes qsTr/tr).
4. Test sur machine réelle : page Config → Update, popup auto (~2 min après boot), upgrade single/groupe/all, restart/reboot post-upgrade.

## Notes d'environnement

- Build desktop complet impossible localement (module Qt6 `core5compat` absent) →
  vérification par `g++ -fsyntax-only` avec les modules Qt6 dispo ; build réel via CI/docker.
- calaos-container : `make` + `make test` possibles (go 1.22, meson, ninja présents).
- **Ne jamais pusher calaos-container** (push sur main = auto-release). Commit local, push manuel.
