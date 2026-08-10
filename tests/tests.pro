#Calaos Home — projet de tests unitaires (Qt Test)
#
# Projet qmake AUTONOME : il n'est inclus ni par desktop.pro ni par mobile.pro.
#
# Utilisation :
#   mkdir -p /tmp/calaos_tests_build && cd /tmp/calaos_tests_build
#   qmake6 <chemin_du_depot>/tests/tests.pro
#   make -j$(nproc)
#   make check
#
# Ajout d'un test : créer tests/tst_<nom>/ puis ajouter <nom> à SUBDIRS.
# Voir tests/README.md.

TEMPLATE = subdirs

SUBDIRS += \
    tst_common \
    tst_eventlogmodel \
    tst_homemodel \
    tst_roomfiltermodel
