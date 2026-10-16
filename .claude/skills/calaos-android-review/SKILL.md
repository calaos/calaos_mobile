---
name: calaos-android-review
description: Review checklist and report format for reviewing an Android (Qt 6) ticket of Calaos Home — scope vs ticket, build reproducibility, Gradle/manifest correctness, JNI/Java safety, security, and non-regression of iOS/desktop. Use when acting as the reviewer of an AND-* ticket diff.
---

# Reviewing an AND-* ticket

You are **read-only on source files**: do not edit, commit or push. You may run builds, `adb`, `git diff/log/show` and read anything. Also read the skills `qt6-android-build` and `qt6-android-jni` in `.claude/skills/`.

## Inputs

The ticket ID and the commit range (e.g. `git diff <base>..HEAD`). Read the ticket in `docs/tasks/`, then the diff, then the surrounding code the diff touches.

## Checklist

1. **Scope**: the diff does what the ticket asks, all acceptance criteria are addressed, nothing out of scope slipped in, ticket files untouched, `3rd_party/` untouched.
2. **Reproduce**: run the build yourself (`tools/build_android.sh Debug`). If a phone is connected (`adb devices`), install and launch, and check `adb logcat` for crashes, `JNI DETECTED ERROR`, `UnsatisfiedLinkError`, `FATAL`. Do not take the implementer's report on trust.
3. **Non-regression**: if `CMakeLists.txt` or `cmake/` changed, run `tools/build_ios.sh Debug` too. `tools/check_sources.sh` must not report missing files on the CMake side. Shared `src/` changes must stay compatible with the desktop build (guards).
4. **Build system**: versions match the matrix in `qt6-android-build`; no stale Qt 5 leftovers (`qt5AndroidDir`, `libs.xml`, Gradle wrapper in `android/`, `compile` deps, `jcenter`); nothing machine-specific hard-coded except through overridable variables; 16 KB alignment when native libs change.
5. **JNI / Java**: rules of `qt6-android-jni` — threading, exception clearing, class loader, null-safety, API-level guards, no natives called from code that can run without Qt.
6. **Security**: no secrets committed, no credentials or full tokens logged, minimal manifest permissions, TLS behavior explicit and justified, exported components only where needed.
7. **Quality**: readable, matches the surrounding code style, no dead code or leftover debug logs, English comments.

## Report format (your final message)

```
VERDICT: APPROVE | NEEDS-CHANGES
Build reproduced: yes/no (command + result) · Device test: yes/no (what was observed)
Findings:
- [blocker|major|minor|nit] path/to/file:line — problem → expected fix
```

`blocker`/`major` = must fix before `Terminé`. Be concrete; no findings without a file:line and a reason.
