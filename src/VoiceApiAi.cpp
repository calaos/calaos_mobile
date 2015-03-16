#include "VoiceApiAi.h"
#include <QStandardPaths>

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
        //do request...
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

}

void VoiceApiAi::netRequestError(QNetworkReply::NetworkError code)
{

}

