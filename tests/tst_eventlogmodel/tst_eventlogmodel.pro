TEMPLATE = app
TARGET = tst_eventlogmodel

include(../common.pri)

# EventLogIO::resolve() (voir T05) est un gabarit entierement defini dans
# EventLogModel.h : ce test l'utilise directement par #include, sans le
# lister dans HEADERS (donc sans moc) et sans lier EventLogModel.cpp.
#
# EventLogModel.h declare aussi les classes QObject EventLogModel/EventLogItem
# et inclut RoomModel.h (IOCache/IOBase) via EventLogModel.cpp seulement.
# IOBase::IOBase() est defini dans RoomModel.cpp, qui reference aussi
# QFAppDispatcher (3rd_party/quickflux) pour RoomModel/ScenarioModel ; et
# CalaosConnection.cpp appelle HardwareUtils::Instance(), dont l'implementation
# desktop tire QApplication/QMessageBox et de nouveau quickflux. Lier l'un de
# ces .cpp pour ce seul test ferait entrer toute cette chaine en cascade — ce
# que tests/README.md deconseille explicitement. Ce test se limite donc a la
# fonction pure EventLogIO::resolve(), qui porte exactement le motif corrige
# par T05 (voir tst_eventlogmodel.cpp pour le detail).
SOURCES += \
    tst_eventlogmodel.cpp
