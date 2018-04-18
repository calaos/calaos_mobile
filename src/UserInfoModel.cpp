#include "UserInfoModel.h"
#include "HardwareUtils.h"

UserInfoModel::UserInfoModel():
    QStandardItemModel()
{
    QHash<int, QByteArray> roles;
    roles[RoleEmail] = "email";
    setItemRoleNames(roles);
}

void UserInfoModel::load()
{
    QString e = HardwareUtils::Instance()->getConfigOption("user_emails");
    if (e.isEmpty())
        return;

    QStringList emails = e.split(',');

    for (const QString &email: qAsConst(emails))
    {
        QStandardItem *it = new QStandardItem();
        it->setText(email);
        it->setData(email, RoleEmail);
        appendRow(it);
    }
}

void UserInfoModel::save()
{
    QStringList emails;
    for (int i = 0;i < rowCount();i++)
        emails.append(item(i)->text());

    HardwareUtils::Instance()->setConfigOption("user_emails", emails.join(','));
}

void UserInfoModel::deleteEmail(int idx)
{
    if (idx < 0 || idx >= rowCount())
        return;
    removeRow(idx);
    save();
}

void UserInfoModel::addEmail(QString email)
{
    QStandardItem *it = new QStandardItem();
    it->setText(email);
    it->setData(email, RoleEmail);
    appendRow(it);
    save();
}

void UserInfoModel::sendEmail(QString subject, QString body)
{
    for (int i = 0;i < rowCount();i++)
        sendEmail(item(i)->text(), subject, body);
}

void UserInfoModel::sendEmail(QString email, QString subject, QString body)
{
    QTemporaryFile mailTemp;
    mailTemp.open();
    mailTemp.setAutoRemove(false);
    mailTemp.write(body.toUtf8());

    QStringList args;
    args.append("--delete");
    args.append("--from");
    args.append("noreply@calaos.fr");
    args.append("--to");
    args.append(email);
    args.append("--subject");
    args.append(subject);
    args.append("--body");
    args.append(mailTemp.fileName());

    qDebug() << "Starting calaos_mail with: " << args;
    QProcess mailProc;
    mailProc.startDetached("calaos_mail", args);
}
