#ifndef VOICEAPIAI_H
#define VOICEAPIAI_H

#include <QtCore>
#include <QAudioInput>
#include <QtNetwork>
#include "qqmlhelpers.h"
#include "Common.h"

class VoiceApiAi: public QObject
{
    Q_OBJECT

    QML_READONLY_PROPERTY(Common::VoiceStatus, voiceStatus)
    QML_READONLY_PROPERTY(double, voiceLevel)
    QML_READONLY_PROPERTY(QString, resultText)

public:
    VoiceApiAi(QObject *parent);
    virtual ~VoiceApiAi();

    Q_INVOKABLE void startVoiceRecord();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void stopRecording();

signals:
    void recordFailed();

    void actionIO(QString ioname, QString action, bool plural);
    void actionIORoom(QString ioname, QString action, QString room_name, bool plural);

private slots:
    void handleStateChanged(QAudio::State state);
    void recordData();

    void sslErrors(QNetworkReply *reply, const QList<QSslError> &);
    void netRequestFinished();
    void netRequestError(QNetworkReply::NetworkError code);

private:
    void doRequest();

    void processJson(const QVariantMap &vmap);

    QNetworkAccessManager *accessManager;
    QNetworkReply *netReply = nullptr;
    QAudioInput *audioInput = nullptr;
    QTemporaryFile *recordFile;
    QTimer *recTimer = nullptr;
};

#endif // VOICEAPIAI_H
