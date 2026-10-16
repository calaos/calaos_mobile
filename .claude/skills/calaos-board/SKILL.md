---
name: calaos-board
description: Workflow for executing a Calaos Home ticket from docs/BOARD.md (tickets in docs/tasks/, status kept only in the board, commit conventions, scope rules, end-of-task report). Use whenever you implement or fix a ticket (T.., AND-..) in calaos_mobile.
---

# Executing a Calaos Home ticket

Repo: `/Users/raoul/work/calaos_mobile`. Read `AGENTS.md` once for the project map.

## Rules

1. Read the ticket file `docs/tasks/<ID>-*.md` fully, plus only the files it lists. Ticket files are **immutable specs** — do not edit them.
2. Status lives **only** in `docs/BOARD.md`: `À faire` → `En cours` → `En revue` → `Terminé`. Set `En cours` when you start and `En revue` when you hand off. Only the orchestrator sets `Terminé` after review.
3. Stay on the current branch (the orchestrator created it). Do not create branches, do not push, do not merge.
4. Stay in scope: anything the ticket marks out of scope, or that you find along the way, goes into your report as a follow-up instead of the diff.
5. Never modify `3rd_party/`. Leave untracked files you did not create alone (e.g. `.github/copilot-instructions.md`, `.github/hooks/`).
6. Both variants exist: Android/iOS changes must not break the desktop qmake build (`desktop.pro`) or the iOS CMake build (`tools/build_ios.sh Debug`). Guard platform code (`if(ANDROID)` / `if(IOS)` in CMake, `#ifdef Q_OS_ANDROID` in C++).
7. New shared C++ source → register it in both `calaos.pri` and `cmake/CalaosHomeSources.cmake` (`tools/check_sources.sh` must stay clean). New QML → the right `.qrc`.
8. Code, comments, identifiers and commit messages in **English**. Docs in `doc/` are in English; tickets and board are in French.
9. Commits: conventional prefix (`feat:`, `fix:`, `build:`, `docs:`, `test:`, `refactor:`), one or more focused commits per ticket, with the ticket ID in the subject, e.g. `build: Android toolchain script and CMake fixes (AND-1)`. No AI attribution trailer.
10. Never commit secrets (keystores, passwords). Never log credentials.

## Done means

- Every acceptance criterion in the ticket is verified **by running it** (real build, real device when the ticket says so), not assumed.
- The board row is `En revue`, the work is committed.

## Final report (your last message)

- What changed (files) and the commit hashes.
- For each acceptance criterion: PASS/FAIL + the evidence (command + key output line).
- Anything left undone, risks, follow-ups.
