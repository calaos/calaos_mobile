# cmake/Versioning.cmake
# Reads the VERSION file and exposes:
#   CALAOS_VERSION       — semver string, e.g. "3.1.1"
#   CALAOS_VERSION_CODE  — integer, e.g. 30101 (MAJOR*10000 + MINOR*100 + PATCH)

file(STRINGS "${CMAKE_SOURCE_DIR}/VERSION.txt" CALAOS_VERSION
    LIMIT_COUNT 1
    REGEX "^[0-9]+\\.[0-9]+\\.[0-9]+$")

if(NOT CALAOS_VERSION)
    message(FATAL_ERROR "VERSION.txt file is missing or does not contain a valid semver string (MAJOR.MINOR.PATCH)")
endif()

string(REPLACE "." ";" _version_parts "${CALAOS_VERSION}")
list(GET _version_parts 0 CALAOS_VERSION_MAJOR)
list(GET _version_parts 1 CALAOS_VERSION_MINOR)
list(GET _version_parts 2 CALAOS_VERSION_PATCH)

math(EXPR CALAOS_VERSION_CODE
    "${CALAOS_VERSION_MAJOR} * 10000 + ${CALAOS_VERSION_MINOR} * 100 + ${CALAOS_VERSION_PATCH}")

message(STATUS "Calaos version: ${CALAOS_VERSION} (code: ${CALAOS_VERSION_CODE})")
