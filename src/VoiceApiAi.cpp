#include "VoiceApiAi.h"
#include <QStandardPaths>

#define API_AI_URL                      "https://api.api.ai/v1/query"
#define API_AI_CLIENT_ACCESS_TOKEN      "ef23f5ee81c1495eaca1daa5cb1906e0"
#define API_AI_DEV_ACCESS_TOKEN         "457c551a032f41ec8a2752d23be2f144"
#define API_AI_SUBS_KEY                 "558d5d04-0c13-41ba-bb8a-99873c1a6f2d"

VoiceApiAi::VoiceApiAi(QObject *parent):
    QObject(parent)
{
    accessManager = new QNetworkAccessManager(this);
    connect(accessManager, SIGNAL(sslErrors(QNetworkReply*, const QList<QSslError> &)),
            this, SLOT(sslErrors(QNetworkReply*, const QList<QSslError> &)));

    update_voiceStatus(Common::VoiceStatusIdle);
}

VoiceApiAi::~VoiceApiAi()
{
    QFile::remove(QString("%1/record.wav").arg(
                      QStandardPaths::writableLocation(QStandardPaths::TempLocation)));
}

void VoiceApiAi::sslErrors(QNetworkReply *reply, const QList<QSslError> &)
{
    reply->ignoreSslErrors();
}

void VoiceApiAi::startVoiceRecord()
{
    if (get_voiceStatus() != Common::VoiceStatusIdle &&
        get_voiceStatus() != Common::VoiceStatusFailure)
        return;

    recordFile = new QTemporaryFile(this);
    recordFile->open();

    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleSize(16);
    format.setCodec("audio/pcm");
    //format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);

    QAudioDeviceInfo info = QAudioDeviceInfo::defaultInputDevice();
    if (!info.isFormatSupported(format))
    {
        qWarning() << "Default format not supported, trying to use the nearest.";
        format = info.nearestFormat(format);
        qWarning() << format;
    }

    audioInput = new QAudioInput(format, this);
    connect(audioInput, SIGNAL(stateChanged(QAudio::State)),
            this, SLOT(handleStateChanged(QAudio::State)));

    // Records audio for 5000ms max
    recTimer = new QTimer(this);
    connect(recTimer, SIGNAL(timeout()), this, SLOT(stopRecording()));
    recTimer->start(5000);
    audioInput->start(recordFile);

    qDebug() << "start recording";

    update_voiceStatus(Common::VoiceStatusRecording);
}

void VoiceApiAi::stopRecording()
{
    if (get_voiceStatus() == Common::VoiceStatusRecording)
    {
        audioInput->stop();
        delete audioInput;
        delete recTimer;
        audioInput = nullptr;
        recTimer = nullptr;

        recordFile->seek(0);

        doRequest();

        return;
    }

    update_voiceStatus(Common::VoiceStatusIdle);
}

void VoiceApiAi::cancel()
{
    //TODO
}

void VoiceApiAi::handleStateChanged(QAudio::State state)
{
    switch (state)
    {
    case QAudio::StoppedState:
        if (audioInput->error() != QAudio::NoError)
        {
            qDebug() << "Recording error";
            update_voiceStatus(Common::VoiceStatusFailure);
            audioInput->stop();
            delete audioInput;
            delete recTimer;
            delete recordFile;
            audioInput = nullptr;
            recTimer = nullptr;
        }
        break;
    case QAudio::ActiveState: break;
    default: break;
    }
}

void VoiceApiAi::netRequestFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply * >(sender());
    if (!reply) return;

    QJsonParseError err;
    QJsonDocument jdoc;

    if (reply->error() != QNetworkReply::NoError)
    {
        update_voiceStatus(Common::VoiceStatusFailure);
        qDebug() << "Error in" << reply->url() << ":" << reply->errorString();
        reply->deleteLater();
        return;
    }

    QByteArray res = reply->readAll();
    jdoc = QJsonDocument::fromJson(res, &err);

    if (err.error != QJsonParseError::NoError)
        qWarning() << "JSON parse error " << err.errorString();
    else
        qDebug() << jdoc.toJson();

    reply->deleteLater();

    update_voiceStatus(Common::VoiceStatusIdle);
}

void VoiceApiAi::netRequestError(QNetworkReply::NetworkError code)
{

}

void VoiceApiAi::doRequest()
{
    update_voiceStatus(Common::VoiceStatusSending);

    QNetworkRequest request;
    request.setUrl(QUrl(API_AI_URL));

    request.setRawHeader("Authorization", "Bearer " API_AI_CLIENT_ACCESS_TOKEN);
    request.setRawHeader("ocp-apim-subscription-key", API_AI_SUBS_KEY);

    QHttpMultiPart *multipart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart jsonpart;
    jsonpart.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    jsonpart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"request\""));
    jsonpart.setBody("{'timezone':'Europe/Paris', 'lang':'fr'}");
    multipart->append(jsonpart);

    QHttpPart wavpart;
    wavpart.setHeader(QNetworkRequest::ContentTypeHeader, "audio/wav");
    wavpart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"voiceData\""));
    wavpart.setBodyDevice(recordFile);
    multipart->append(wavpart);

    QNetworkReply *reply = accessManager->post(request, multipart);
    connect(reply, SIGNAL(finished()),
            this, SLOT(netRequestFinished()));

    multipart->setParent(reply); // delete the multiPart with the reply
}
