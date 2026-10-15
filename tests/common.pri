#Configuration partagée par tous les tests unitaires Calaos.
#
# Chaque tests/tst_<nom>/tst_<nom>.pro fait include(../common.pri) puis ajoute
# UNIQUEMENT les sources de src/ dont il a réellement besoin (via $$SRC_DIR).
# Ne jamais lier l'ensemble de calaos.pri : certains fichiers tirent des
# dépendances lourdes (HardwareUtils, quickflux, QtQuick, ressources Qt).

QT += testlib core gui network websockets qml
CONFIG += c++17 console testcase
CONFIG -= app_bundle

# Les binaires de test ne sont pas des livrables : ne rien installer.
CONFIG += no_testcase_installs

# Racine des sources applicatives, à utiliser dans les .pro des tests.
SRC_DIR = $$PWD/../src

INCLUDEPATH += $$SRC_DIR

# Artefacts intermédiaires regroupés (garde les répertoires de build lisibles).
MOC_DIR = $$OUT_PWD/.moc
OBJECTS_DIR = $$OUT_PWD/.obj
RCC_DIR = $$OUT_PWD/.rcc
