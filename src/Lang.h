#ifndef LANG_H
#define LANG_H

#include <QObject>
#include <QStandardItemModel>
#include <QQmlApplicationEngine>
#include "qqmlhelpers.h"
#include "HardwareUtils.h"

class LangModel: public QStandardItemModel
{
    Q_OBJECT

    QML_READONLY_PROPERTY(bool, hasDynamicLangChange)

public:
    LangModel(QQmlApplicationEngine *engine, QObject *parent = 0);

    enum
    {
        RoleCode = Qt::UserRole + 1,
        RoleName,
        RoleActive,
    };

    void load();

    Q_INVOKABLE QObject *getLangModel(int idx) const;

private:
    QQmlApplicationEngine *engine;
};

class Lang: public QObject, public QStandardItem
{
    Q_OBJECT

    QML_READONLY_PROPERTY_MODEL(QString, langCode, LangModel::RoleCode)
    QML_READONLY_PROPERTY_MODEL(QString, langName, LangModel::RoleName)
    QML_WRITABLE_PROPERTY_MODEL(bool, langActive, LangModel::RoleActive)

public:
    Lang();
    virtual ~Lang() {}
};

#endif // LANG_H
