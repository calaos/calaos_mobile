# cmake/CalaosHomeSources.cmake
# Shared C++ source list for the mobile (iOS + Android) CMake build.
# This is the mobile counterpart of calaos.pri SOURCES/HEADERS.
# Keep in sync with calaos.pri until the desktop variant also migrates.
# Regression check: tools/check_sources.sh diffs the two lists at CI time.

set(CALAOS_SOURCES
    src/main.cpp
    src/HomeModel.cpp
    src/Application.cpp
    src/CalaosConnection.cpp
    src/CalaosEventDecoder.cpp
    src/Common.cpp
    src/IOTypeRegistry.cpp
    src/RoomModel.cpp
    src/AudioModel.cpp
    src/FavoritesModel.cpp
    src/HardwareUtils.cpp
    src/CameraModel.cpp
    src/RoomFilterModel.cpp
    src/Machine.cpp
    src/Lang.cpp
    src/EventLogModel.cpp
    src/BookmarkModel.cpp
    src/ModelImageProvider.cpp
    src/TlsPinning.cpp
    src/CalaosOsProtocol.cpp
    src/UpdateModel.cpp
    src/AdminRelay.cpp
    src/AdminRelayBackend.cpp
    src/AdminUpdates.cpp
)

set(CALAOS_HEADERS
    src/HomeModel.h
    src/Application.h
    src/CalaosConnection.h
    src/CalaosEventDecoder.h
    src/Common.h
    src/IOTypeRegistry.h
    src/IOConnection.h
    src/JsonKeys.h
    src/RoomModel.h
    src/AudioModel.h
    src/FavoritesModel.h
    src/HardwareUtils.h
    src/LaunchLogin.h
    src/CameraModel.h
    src/RoomFilterModel.h
    src/Machine.h
    src/qqmlhelpers.h
    src/Lang.h
    src/EventLogModel.h
    src/BookmarkModel.h
    src/ModelImageProvider.h
    src/TlsPinning.h
    src/CalaosOsProtocol.h
    src/UpdateBackend.h
    src/UpdateModel.h
    src/UpdateGrouping.h
    src/AdminRelay.h
    src/AdminRelayBackend.h
    src/AdminUpdates.h
)
