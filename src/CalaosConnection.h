#ifndef CALAOSCONNECTION_H
#define CALAOSCONNECTION_H

#include <QtCore>
#include <QtNetwork>

class CalaosConnection : public QObject
{
    Q_OBJECT
public:
    explicit CalaosConnection(QObject *parent = 0);

private:
    QNetworkAccessManager *accessManager = nullptr;

signals:
    void homeLoaded(QVariantMap &home);
    void disconnected();
    void loginFailed();

public slots:
    void login(QString user, QString pass, QString host);

private slots:
    void sslErrors(QNetworkReply *reply, const QList<QSslError> &);
    void loginFinished(QNetworkReply *reply);
    void requestFinished(QNetworkReply *reply);

};

#endif // CALAOSCONNECTION_H
