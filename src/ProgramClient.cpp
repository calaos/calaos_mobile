#include "ProgramClient.h"

#include <QJsonArray>

ProgramClient::ProgramClient(ProgramTransport *t, QObject *source, QObject *parent):
    QObject(parent),
    transport(t)
{
    connect(source, SIGNAL(trackedRequestFinished(QString,bool,QVariantMap)),
            this, SLOT(onTrackedRequestFinished(QString,bool,QVariantMap)));
    connect(source, SIGNAL(eventTimeRangeChange(QString)),
            this, SIGNAL(timeRangeChanged(QString)));
}

QString ProgramClient::getTimeRange(const QString &ioId)
{
    return send(Kind::GetTimeRange, QStringLiteral("get_timerange"), QJsonObject{{ "id", ioId }});
}

QString ProgramClient::setTimeRange(const QString &ioId, const QVariantList &ranges, const QString &months)
{
    QJsonObject data{{ "id", ioId },
                     { "months", months },
                     { "ranges", QJsonArray::fromVariantList(ranges) }};
    return send(Kind::SetTimeRange, QStringLiteral("set_timerange"), data);
}

QString ProgramClient::getStates(const QStringList &ioIds)
{
    return send(Kind::GetStates, QStringLiteral("get_state"),
                QJsonObject{{ "items", QJsonArray::fromStringList(ioIds) }});
}

QString ProgramClient::setState(const QString &ioId, const QString &value)
{
    return send(Kind::SetState, QStringLiteral("set_state"),
                QJsonObject{{ "id", ioId }, { "value", value }});
}

QString ProgramClient::send(Kind kind, const QString &action, const QJsonObject &data)
{
    const QString id = transport->sendTrackedRequest(action, data);
    if (id.isEmpty())
        return id;

    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, [this, id]()
    {
        transport->cancelTrackedRequest(id);
        finish(id, false, QVariantMap(), QStringLiteral("timeout"));
    });
    timer->start(timeoutMs);
    pending.insert(id, { kind, timer });
    return id;
}

void ProgramClient::onTrackedRequestFinished(const QString &requestId, bool transportOk, const QVariantMap &answer)
{
    const auto it = pending.constFind(requestId);
    if (it == pending.constEnd())
        return;

    if (!transportOk)
    {
        finish(requestId, false, QVariantMap(), QStringLiteral("transport"));
        return;
    }

    const QString error = answer.value(QStringLiteral("error")).toString();
    if (!error.isEmpty())
    {
        finish(requestId, false, answer, error);
        return;
    }

    switch (it->kind)
    {
    case Kind::GetTimeRange:
        if (answer.value(QStringLiteral("ranges")).typeId() != QMetaType::QVariantList)
            finish(requestId, false, answer, QStringLiteral("invalid ranges"));
        else
            finish(requestId, true, answer, QString());
        break;
    case Kind::GetStates:
        finish(requestId, true, answer, QString());
        break;
    case Kind::SetTimeRange:
    case Kind::SetState:
        //set_state only says "false", for an unknown IO as for a refused value.
        if (answer.value(QStringLiteral("success")).toString() == QLatin1String("true"))
            finish(requestId, true, answer, QString());
        else
            finish(requestId, false, answer, QStringLiteral("failed"));
        break;
    }
}

void ProgramClient::finish(const QString &requestId, bool ok, const QVariantMap &answer, const QString &error)
{
    const auto it = pending.find(requestId);
    if (it == pending.end())
        return;
    it->timer->stop();
    it->timer->deleteLater();
    pending.erase(it);
    emit finished(requestId, ok, answer, error);
}
