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
    QNetworkAccessManager *accessManager;

    QString username, password;
    QString host;
    QString uuidPolling;

    QList<QNetworkReply *> reqReplies;
    QNetworkReply *pollReply;

    void processEvents(QString msg);

signals:
    void homeLoaded(QVariantMap &home);
    void disconnected();
    void loginFailed();

    //events signals
    void eventInputChange(QString id, QString state, QString value);
    void eventOutputChange(QString id, QString state, QString value);
    void eventInputAdd(QString id);
    void eventOutputAdd(QString id);
    void eventInputDel(QString id);
    void eventOutputDel(QString id);
    void eventRoomChange(QString name, QString type, QString value);
    void eventRoomAdd(QString name, QString type);
    void eventRoomDel(QString name, QString type);
    void eventAudioChange(QString playerid);
    void eventAudioPlaylistChange(QString playerid);
    void eventAudioStatusChange(QString playerid, QString status);
    void eventAudioVolumeChange(QString playerid, double volume);
    void eventAudioStateChange(const QVariantMap &data);
    void eventInputStateChange(const QVariantMap &data);
    void eventOutputStateChange(const QVariantMap &data);
    void eventScenarioNew();
    void eventScenarioDel();
    void eventScenarioChange();

public slots:
    void login(QString user, QString pass, QString host);
    void logout();
    void sendCommand(QString id, QString value, QString type = QString(), QString action = QString());
    void queryState(QStringList inputs, QStringList outputs, QStringList audio_players);

private slots:
    void sslErrors(QNetworkReply *reply, const QList<QSslError> &);
    void loginFinished(QNetworkReply *reply);

    void requestFinished();
    void requestError(QNetworkReply::NetworkError code);

    void startJsonPolling();
};

#endif // CALAOSCONNECTION_H
