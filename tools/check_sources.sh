#!/usr/bin/env bash
# tools/check_sources.sh
# Regression test: verify that cmake/CalaosHomeSources.cmake and calaos.pri
# declare the same shared C++ source files.
#
# Only the shared (unconditional) lists are compared:
#   - calaos.pri: SOURCES/HEADERS assignments at top level (not inside a
#     platform block such as `win32 { ... }`), `$$PWD/` prefixes stripped,
#     entries on the `SOURCES +=` line and backslash continuations included.
#   - cmake/CalaosHomeSources.cmake: the CALAOS_SOURCES and CALAOS_HEADERS
#     set() lists (platform-specific appends are not part of them).
#
# Usage: ./tools/check_sources.sh
# Exit code: 0 = in sync, 1 = divergence detected.
# Portable: bash 3.2 (macOS /bin/bash) and GNU/Linux; POSIX awk only.

set -euo pipefail
export LC_ALL=C

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Extract source/header paths from the top-level SOURCES/HEADERS assignments
# of calaos.pri. Braced blocks (platform-specific) are skipped.
qmake_sources() {
    awk '
    {
        line = $0
        sub(/#.*/, "", line)
        if (depth == 0) {
            if (inlist || line ~ /^[[:space:]]*(SOURCES|HEADERS)[[:space:]]*[+]?=/) {
                sub(/^[[:space:]]*(SOURCES|HEADERS)[[:space:]]*[+]?=/, "", line)
                inlist = (line ~ /\\[[:space:]]*$/)
                sub(/\\[[:space:]]*$/, "", line)
                n = split(line, toks, /[[:space:]]+/)
                for (i = 1; i <= n; i++) {
                    t = toks[i]
                    sub(/^\$\$PWD\//, "", t)
                    if (t != "") print t
                }
            } else {
                inlist = 0
            }
        }
        depth += (split(line, o, "{") - 1) - (split(line, c, "}") - 1)
    }
    ' "$REPO_ROOT/calaos.pri" | sort
}

# Extract source/header paths from the CALAOS_SOURCES and CALAOS_HEADERS
# set() lists of the CMake file.
cmake_sources() {
    awk '
    {
        line = $0
        sub(/#.*/, "", line)
        if (!inblk && line ~ /^[[:space:]]*set[[:space:]]*\([[:space:]]*CALAOS_(SOURCES|HEADERS)/) {
            inblk = 1
            sub(/^[[:space:]]*set[[:space:]]*\([[:space:]]*CALAOS_(SOURCES|HEADERS)/, "", line)
        }
        if (inblk) {
            closing = (line ~ /\)/)
            sub(/\).*$/, "", line)
            n = split(line, toks, /[[:space:]]+/)
            for (i = 1; i <= n; i++) {
                if (toks[i] != "") print toks[i]
            }
            if (closing) inblk = 0
        }
    }
    ' "$REPO_ROOT/cmake/CalaosHomeSources.cmake" | sort
}

QMAKE=$(qmake_sources)
CMAKE=$(cmake_sources)

DIFF=$(diff <(printf '%s\n' "$QMAKE") <(printf '%s\n' "$CMAKE") || true)

if [[ -z "$DIFF" ]]; then
    echo "OK: calaos.pri and cmake/CalaosHomeSources.cmake are in sync."
    exit 0
else
    echo "ERROR: source list divergence detected between calaos.pri and cmake/CalaosHomeSources.cmake"
    echo ""
    echo "--- calaos.pri   (+lines present in qmake, missing from cmake)"
    echo "+++ CalaosHomeSources.cmake  (+lines present in cmake, missing from qmake)"
    echo "$DIFF"
    exit 1
fi
