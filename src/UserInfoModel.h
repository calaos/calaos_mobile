#ifndef USERINFOMODEL_H
#define USERINFOMODEL_H

#include <QStandardItemModel>
#include <QObject>
#include "qqmlhelpers.h"

class UserInfoModel : public QStandardItemModel
{
    Q_OBJECT
public:
    static UserInfoModel *Instance()
    {
        static UserInfoModel inst;
        return &inst;
    }

    enum
    {
        RoleEmail = Qt::UserRole + 1,
    };

    Q_INVOKABLE void load();
    Q_INVOKABLE void save();

    Q_INVOKABLE void addEmail(QString email);
    Q_INVOKABLE void deleteEmail(int idx);

    Q_INVOKABLE bool isEmpty() { return rowCount() == 0; }

    //Send an email to all registered email address using calaos_mail utility
    Q_INVOKABLE void sendEmail(QString subject, QString body);

private:
    UserInfoModel();

    void sendEmail(QString email, QString subject, QString body);
};

#endif // USERINFOMODEL_H
