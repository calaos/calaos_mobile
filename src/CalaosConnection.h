#ifndef CALAOSCONNECTION_H
#define CALAOSCONNECTION_H

#include <QtCore>
#include <QtNetwork>
#include <QWebSocket>

class CalaosConnection : public QObject
{
    Q_OBJECT
public:
    explicit CalaosConnection(QObject *parent = 0);

    enum
    {
        ConStateUnknown,
        ConStateTryWebsocket,
        ConStateHttp,
        ConStateWebsocket,
    };

    void updateHttpApiV2(bool en) { isV2HttpApi = en; }
    bool isHttpApiV2() { return isV2HttpApi; }
    bool isWebsocket() { return wsocket && constate == ConStateWebsocket; }
    bool isHttp() { return constate == ConStateHttp; }

    void sendJson(QString action, QJsonObject &jsonData);

    QString getNotifPictureUrl(const QString &pic_uid);

    void changeCredentials(QString user, QString pass);

private:
    QNetworkAccessManager *accessManager;

    //Only for queuing camera requests, on desktop max 6 // requests
    //This is to prevent blocking the main accessManager for the cameras
    QNetworkAccessManager *accessManagerCam;

    QString username, password;
    QString wshost, httphost;
    QString uuidPolling;

    //used when changing credentials
    QString username_temp, password_temp;

    int constate = ConStateUnknown;

    QList<QNetworkReply *> reqReplies;
    QNetworkReply *pollReply;

    QWebSocket *wsocket = nullptr;

    bool isV2HttpApi = true;

    QTimer *wsPingTimeout = nullptr;
    QTimer *wsPing = nullptr;

    bool demoMode = false;

    void connectWebsocket(QString h);
    void closeWebsocket();
    void connectHttp(QString h);
    void sendWebsocket(const QString &msg, const QJsonObject &data = QJsonObject(), const QString &client_id = QString());
    void sendHttp(const QString &msg, QJsonObject &data, bool ignoreErrors = true);

    void processEventsV2(QString msg);
    void processEventsV3(QVariantMap msg);

signals:
    void homeLoaded(const QVariantMap &home);
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
    void eventAudioStateChange(QString playerid, const QVariantMap &data);
    void eventInputStateChange(const QVariantMap &data);
    void eventOutputStateChange(const QVariantMap &data);
    void eventScenarioNew();
    void eventScenarioDel();
    void eventScenarioChange();
    void cameraPictureDownloaded(const QString &camid, const QByteArray &data);
    void cameraPictureFailed(const QString &camid);
    void eventTouchscreenCamera(QString camid);
    void logEventLoaded(const QVariantMap &data);
    void audioCoverDownloaded(const QString &camid, const QByteArray &data);
    void changeCredsSuccess(QString uname, QString pass);
    void changeCredsFailed();

public slots:
    void login(QString user, QString pass, QString host);
    void logout();
    void sendCommand(QString id, QString value, QString type = QString(), QString action = QString());
    void queryState(QStringList inputs, QStringList outputs, QStringList audio_players);
    void getCameraPicture(const QString &camid, QString urlSuffix = QString());
    void getAudioCover(const QString &playerid);

private slots:
    void sslErrors(QNetworkReply *reply, const QList<QSslError> &);
    void sslErrorsWebsocket(const QList<QSslError> &errors);
    void loginFinished(QNetworkReply *reply);

    void requestFinished();
    void requestCamFinished(QNetworkReply *reqReply, const QString &camid);
    void requestError(QNetworkReply::NetworkError code);
    void requestAudioCoverFinished(QNetworkReply *reqReply, const QString &camid);

    void startJsonPolling();

    //Websocket slots
    void onWsConnected();
    void onWsDisconnected();
    void onWsError();
    void onWsTextMessageReceived(const QString &message);
};

#endif // CALAOSCONNECTION_H
