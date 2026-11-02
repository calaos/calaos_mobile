#include <QtTest>
#include <QApplication>
#include <QFile>
#include <QTranslator>
#include "ControlPanelModel.h"

class TstControlPanelModel: public QObject
{
    Q_OBJECT

private:
    static int rowOf(const ControlPanelModel &m, const QString &clickId)
    {
        for (int i = 0; i < m.rowCount(); i++)
            if (m.data(m.index(i, 0), ControlPanelModel::RoleClickId).toString() == clickId)
                return i;
        return -1;
    }

    static QString role(const ControlPanelModel &m, const QString &clickId, int r)
    {
        return m.data(m.index(rowOf(m, clickId), 0), r).toString();
    }

    static bool installLang(QTranslator &t, const QString &lang)
    {
        return t.load(QStringLiteral(CALAOS_LANG_DIR "/calaos_") + lang) &&
               QCoreApplication::installTranslator(&t);
    }

private slots:
    void clockEntryHasItsOwnIcon()
    {
        ControlPanelModel m;
        QCOMPARE(role(m, "config/time", ControlPanelModel::RoleIcon),
                 QStringLiteral("qrc:/img/icon_config_clock.png"));
        QCOMPARE(role(m, "config/l18n", ControlPanelModel::RoleIcon),
                 QStringLiteral("qrc:/img/icon_config_l18n.png"));

        for (int i = 0; i < m.rowCount(); i++)
        {
            const QString icon = m.data(m.index(i, 0), ControlPanelModel::RoleIcon).toString();
            QVERIFY2(QFile::exists(icon.mid(3)), qPrintable(icon));
        }
    }

    void titlesAreTranslatedWhenModelIsBuiltAfterTheLanguage_data()
    {
        QTest::addColumn<QString>("lang");
        QTest::addColumn<QString>("title");
        QTest::addColumn<QString>("subTitle");
        QTest::newRow("fr") << "fr" << "Date et heure"
                            << "Horloge, fuseau horaire et heure automatique";
        QTest::newRow("de") << "de" << "Datum und Uhrzeit"
                            << "Uhr, Zeitzone und automatische Uhrzeit";
    }

    void titlesAreTranslatedWhenModelIsBuiltAfterTheLanguage()
    {
        QFETCH(QString, lang);
        QFETCH(QString, title);
        QFETCH(QString, subTitle);

        QTranslator t;
        QVERIFY(installLang(t, lang));
        ControlPanelModel m;
        QCOMPARE(role(m, "config/time", ControlPanelModel::RoleTitle), title);
        QCOMPARE(role(m, "config/time", ControlPanelModel::RoleSubTitle), subTitle);
        QCoreApplication::removeTranslator(&t);
    }

    void languageChangedAtRuntimeIsFollowed()
    {
        ControlPanelModel m;
        QCOMPARE(role(m, "config/time", ControlPanelModel::RoleTitle), QStringLiteral("Date and time"));

        QTranslator t;
        QVERIFY(installLang(t, "fr"));
        QSignalSpy changed(&m, &QAbstractItemModel::dataChanged);
        m.retranslate();
        QCOMPARE(role(m, "config/time", ControlPanelModel::RoleTitle), QStringLiteral("Date et heure"));
        QCOMPARE(role(m, "config/screen", ControlPanelModel::RoleTitle), QStringLiteral("Écran de veille"));
        QVERIFY(!changed.isEmpty());

        QCoreApplication::removeTranslator(&t);
        m.retranslate();
        QCOMPARE(role(m, "config/time", ControlPanelModel::RoleTitle), QStringLiteral("Date and time"));
    }

    void optionsAddInstallAndRollbackRows()
    {
        ControlPanelModel m;
        QCOMPARE(rowOf(m, "config/install"), -1);
        QCOMPARE(rowOf(m, "config/rollback"), -1);

        m.setOptions(true, true);
        QVERIFY(rowOf(m, "config/install") >= 0);
        QVERIFY(rowOf(m, "config/rollback") >= 0);

        m.setOptions(false, false);
        QCOMPARE(rowOf(m, "config/install"), -1);
    }
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    TstControlPanelModel t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_controlpanelmodel.moc"
