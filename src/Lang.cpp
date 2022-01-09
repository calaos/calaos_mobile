#include "Lang.h"

LangModel::LangModel(QQmlApplicationEngine *e, QObject *parent):
    QStandardItemModel(parent),
    engine(e)
{
    QHash<int, QByteArray> roles;
    roles[RoleCode] = "langCode";
    roles[RoleName] = "langName";
    roles[RoleActive] = "langActive";
    setItemRoleNames(roles);

    update_hasDynamicLangChange(true);

    load();
}

QObject *LangModel::getLangModel(int idx) const
{
    Lang *it = dynamic_cast<Lang *>(item(idx));
    if (it) engine->setObjectOwnership(it, QQmlEngine::CppOwnership);
    return it;
}

void LangModel::load()
{
    QString language = HardwareUtils::Instance()->getConfigOption("lang");

    Lang *sysDefault = new Lang();
    sysDefault->update_langName(tr("System default"));
    if (language == "")
        sysDefault->set_langActive(true);

    appendRow(sysDefault);

    //Also add base english lang
    Lang *en = new Lang();
    en->update_langCode("en");
    en->update_langName(tr("English"));
    if (language == "en")
        en->set_langActive(true);

    appendRow(en);

    QDirIterator it(":/lang", QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        QString fname = it.next();
        if (!fname.startsWith(":/lang/calaos_")) continue;
        QString llang = fname.section('_', 1).section('.', 0, 0);
        QLocale locale(llang);

        QString ln;
        if (locale.language() == QLocale::Chinese)
            ln = QString("%1 (%2)")
                 .arg(QLocale::languageToString(locale.language()))
                 .arg(QLocale::scriptToString(locale.script()));
        else
            ln = locale.nativeLanguageName();

        Lang *lang = new Lang();
        lang->update_langCode(llang);
        lang->update_langName(ln);

        if (language == llang)
            lang->set_langActive(true);

        appendRow(lang);
    }
}

Lang::Lang():
    QObject(),
    QStandardItem()
{
    set_langActive(false);
}
