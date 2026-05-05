#!/usr/bin/env bash
# tools/check_sources.sh
# Regression test: verify that cmake/CalaosHomeSources.cmake and calaos.pri
# declare the same shared C++ source files.
# Usage: ./tools/check_sources.sh
# Exit code: 0 = in sync, 1 = divergence detected.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Extract .cpp/.mm source paths from calaos.pri (strip qmake variable noise)
qmake_sources() {
    grep -E '^\s+src/' "$REPO_ROOT/calaos.pri" \
        | sed 's/[[:space:]]//g; s/\\$//' \
        | grep -v '^\s*$' \
        | sort
}

# Extract .cpp source paths from cmake/CalaosHomeSources.cmake
cmake_sources() {
    grep -E '^\s+src/' "$REPO_ROOT/cmake/CalaosHomeSources.cmake" \
        | sed 's/[[:space:]]//g' \
        | grep -v '^\s*$' \
        | sort
}

QMAKE=$(qmake_sources)
CMAKE=$(cmake_sources)

DIFF=$(diff <(echo "$QMAKE") <(echo "$CMAKE") || true)

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
