
#Translations rules
LANGUAGES = fr de
	
# parameters: var, prepend, append
defineReplace(prependAll) {
	for(a,$$1):result += $$2$${a}$$3
	return($$result)
}

TRANSLATIONS = $$prependAll(LANGUAGES, $$PWD/calaos_, .ts)


#Build *.qm translation files automatically
#can be disabled with CONFIG+=DISABLE_QM
!DISABLE_QM {
    isEmpty(QMAKE_LRELEASE) {
        QMAKE_LRELEASE = $$[QT_INSTALL_BINS]/lrelease
    }

    updateqm.input = TRANSLATIONS
    updateqm.output = ${QMAKE_FILE_PATH}/${QMAKE_FILE_BASE}.qm
    updateqm.commands = $$QMAKE_LRELEASE ${QMAKE_FILE_IN} -qm ${QMAKE_FILE_PATH}/${QMAKE_FILE_BASE}.qm
    updateqm.CONFIG += no_link
    QMAKE_EXTRA_COMPILERS += updateqm
    PRE_TARGETDEPS += compiler_updateqm_make_all
}
