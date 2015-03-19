#include "VoiceApiAi.h"
#include <QStandardPaths>
#include <QAudioBuffer>

#define API_AI_URL                      "https://api.api.ai/v1/query"
#define API_AI_CLIENT_ACCESS_TOKEN      "ef23f5ee81c1495eaca1daa5cb1906e0"
#define API_AI_DEV_ACCESS_TOKEN         "457c551a032f41ec8a2752d23be2f144"
#define API_AI_SUBS_KEY                 "558d5d04-0c13-41ba-bb8a-99873c1a6f2d"

static qreal getPeakValue(const QAudioFormat &format);
static QVector<qreal> getBufferLevels(const QAudioBuffer &buffer);

template <class T>
static QVector<qreal> getBufferLevels(const T *buffer, int frames, int channels);

VoiceApiAi::VoiceApiAi(QObject *parent):
    QObject(parent)
{
    accessManager = new QNetworkAccessManager(this);
    connect(accessManager, SIGNAL(sslErrors(QNetworkReply*, const QList<QSslError> &)),
            this, SLOT(sslErrors(QNetworkReply*, const QList<QSslError> &)));

    update_voiceStatus(Common::VoiceStatusIdle);
    update_voiceLevel(0);
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

    QIODevice *dev = audioInput->start();
    connect(dev, SIGNAL(readyRead()), this, SLOT(recordData()));

    qDebug() << "start recording";

    update_voiceStatus(Common::VoiceStatusRecording);
    update_resultText(QString());
}

void VoiceApiAi::recordData()
{
    QIODevice *input = qobject_cast<QIODevice * >(sender());

    QByteArray data = input->read(4096); //read max 4k bytes

    if(data.length() > 0)
    {
        qint64 index;
        qint64 samples = data.length() * 8 / audioInput->format().sampleSize();

        qint16 *d = reinterpret_cast<qint16 *>(data.data());

        //Remove noise using Low Pass filter algorithm
        for (index = 1;index < samples; index++)
        {
            d[index] = 0.333 * d[index] + (1.0 - 0.333) * d[index - 1];
        }

        recordFile->write(data);

        //get peak level value
        QAudioBuffer buffer(data, audioInput->format());
        QVector<qreal> levels = getBufferLevels(buffer);
        if (levels.size() > 0)
            update_voiceLevel(levels.at(0)); //first channel only
    }
}

void VoiceApiAi::stopRecording()
{
    update_voiceLevel(0);

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
    qDebug() << "Cancel";

    if (get_voiceStatus() == Common::VoiceStatusRecording)
    {
        audioInput->stop();
        delete audioInput;
        delete recTimer;
        delete recordFile;
        audioInput = nullptr;
        recTimer = nullptr;
    }

    if (netReply)
    {
        netReply->abort();
        netReply->deleteLater();
        netReply = nullptr;
    }

    update_resultText("");
    update_voiceLevel(0);
    update_voiceStatus(Common::VoiceStatusIdle);
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
    {
        qWarning() << "JSON parse error " << err.errorString();
        update_voiceStatus(Common::VoiceStatusFailure);
    }
    else
    {
        qDebug() << jdoc.toJson();
        processJson(jdoc.object().toVariantMap());
    }

    reply->deleteLater();
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

    netReply = accessManager->post(request, multipart);
    connect(netReply, SIGNAL(finished()),
            this, SLOT(netRequestFinished()));

    multipart->setParent(netReply); // delete the multiPart with the reply
}

void VoiceApiAi::processJson(const QVariantMap &vmap)
{
    if (!vmap.contains("result"))
    {
        update_voiceStatus(Common::VoiceStatusFailure);
        return;
    }

    QVariantMap result = vmap["result"].toMap();

    if (result.contains("resolvedQuery"))
        update_resultText(result["resolvedQuery"].toString());

    if (!result.contains("action"))
    {
        update_voiceStatus(Common::VoiceStatusNotRecognized);
        return;
    }

    update_voiceStatus(Common::VoiceStatusResult);

    QVariantMap params = vmap["parameters"].toMap();
    if (params["room_name"].toString() == "")
    {
        emit actionIO(params["io_name"].toString(),
                      params["action"].toString(),
                      params["art_io"].toString() == "plural");
    }
    else
    {
        emit actionIORoom(params["io_name"].toString(),
                          params["action"].toString(),
                          params["room_name"].toString(),
                          params["art_io"].toString() == "plural");
    }
}

// This function returns the maximum possible sample value for a given audio format
qreal getPeakValue(const QAudioFormat& format)
{
    // Note: Only the most common sample formats are supported
    if (!format.isValid())
        return qreal(0);

    if (format.codec() != "audio/pcm")
        return qreal(0);

    switch (format.sampleType()) {
    case QAudioFormat::Unknown:
        break;
    case QAudioFormat::Float:
        if (format.sampleSize() != 32) // other sample formats are not supported
            return qreal(0);
        return qreal(1.00003);
    case QAudioFormat::SignedInt:
        if (format.sampleSize() == 32)
            return qreal(INT_MAX);
        if (format.sampleSize() == 16)
            return qreal(SHRT_MAX);
        if (format.sampleSize() == 8)
            return qreal(CHAR_MAX);
        break;
    case QAudioFormat::UnSignedInt:
        if (format.sampleSize() == 32)
            return qreal(UINT_MAX);
        if (format.sampleSize() == 16)
            return qreal(USHRT_MAX);
        if (format.sampleSize() == 8)
            return qreal(UCHAR_MAX);
        break;
    }

    return qreal(0);
}

// returns the audio level for each channel
QVector<qreal> getBufferLevels(const QAudioBuffer& buffer)
{
    QVector<qreal> values;

    if (!buffer.format().isValid() || buffer.format().byteOrder() != QAudioFormat::LittleEndian)
        return values;

    if (buffer.format().codec() != "audio/pcm")
        return values;

    int channelCount = buffer.format().channelCount();
    values.fill(0, channelCount);
    qreal peak_value = getPeakValue(buffer.format());
    if (qFuzzyCompare(peak_value, qreal(0)))
        return values;

    switch (buffer.format().sampleType()) {
    case QAudioFormat::Unknown:
    case QAudioFormat::UnSignedInt:
        if (buffer.format().sampleSize() == 32)
            values = getBufferLevels(buffer.constData<quint32>(), buffer.frameCount(), channelCount);
        if (buffer.format().sampleSize() == 16)
            values = getBufferLevels(buffer.constData<quint16>(), buffer.frameCount(), channelCount);
        if (buffer.format().sampleSize() == 8)
            values = getBufferLevels(buffer.constData<quint8>(), buffer.frameCount(), channelCount);
        for (int i = 0; i < values.size(); ++i)
            values[i] = qAbs(values.at(i) - peak_value / 2) / (peak_value / 2);
        break;
    case QAudioFormat::Float:
        if (buffer.format().sampleSize() == 32) {
            values = getBufferLevels(buffer.constData<float>(), buffer.frameCount(), channelCount);
            for (int i = 0; i < values.size(); ++i)
                values[i] /= peak_value;
        }
        break;
    case QAudioFormat::SignedInt:
        if (buffer.format().sampleSize() == 32)
            values = getBufferLevels(buffer.constData<qint32>(), buffer.frameCount(), channelCount);
        if (buffer.format().sampleSize() == 16)
            values = getBufferLevels(buffer.constData<qint16>(), buffer.frameCount(), channelCount);
        if (buffer.format().sampleSize() == 8)
            values = getBufferLevels(buffer.constData<qint8>(), buffer.frameCount(), channelCount);
        for (int i = 0; i < values.size(); ++i)
            values[i] /= peak_value;
        break;
    }

    return values;
}

template <class T>
QVector<qreal> getBufferLevels(const T *buffer, int frames, int channels)
{
    QVector<qreal> max_values;
    max_values.fill(0, channels);

    for (int i = 0; i < frames; ++i) {
        for (int j = 0; j < channels; ++j) {
            qreal value = qAbs(qreal(buffer[i * channels + j]));
            if (value > max_values.at(j))
                max_values.replace(j, value);
        }
    }

    return max_values;
}
