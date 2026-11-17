#ifndef PROGRAMCLIENT_H
#define PROGRAMCLIENT_H

#include <QObject>
#include <QHash>
#include <QTimer>
#include <QVariantMap>

#include "ProgramTransport.h"

//Time range and program state requests for the program editor
//(TimeRangeEditor.qml). Every call returns a request id, empty when nothing
//was sent; a non-empty id is followed by exactly one finished().
class ProgramClient: public QObject
{
    Q_OBJECT
public:
    //source carries the transport's signals (see ProgramTransport.h); it is
    //the transport itself for CalaosConnection.
    ProgramClient(ProgramTransport *transport, QObject *source, QObject *parent = nullptr);

    Q_INVOKABLE QString getTimeRange(const QString &ioId);
    //ranges is the whole list of the IO (seven days): the server replaces
    //every range with it, [] clears them all.
    Q_INVOKABLE QString setTimeRange(const QString &ioId, const QVariantList &ranges,
                                     const QString &months = QStringLiteral("111111111111"));
    Q_INVOKABLE QString getStates(const QStringList &ioIds);
    Q_INVOKABLE QString setState(const QString &ioId, const QString &value);

    void setTimeoutMs(int ms) { timeoutMs = ms; }
    int pendingRequests() const { return pending.size(); }

signals:
    //error is empty when ok, otherwise the server's string ("wrong input",
    //"invalid range", "config_not_written"...), "transport" or "timeout".
    //answer: the ranges answer of a get_timerange, the {id: value} map of a
    //get_state.
    void finished(const QString &requestId, bool ok, const QVariantMap &answer, const QString &error);
    void timeRangeChanged(const QString &ioId);

private slots:
    void onTrackedRequestFinished(const QString &requestId, bool transportOk, const QVariantMap &answer);

private:
    enum class Kind { GetTimeRange, SetTimeRange, GetStates, SetState };
    struct Pending
    {
        Kind kind;
        QTimer *timer;
    };

    QString send(Kind kind, const QString &action, const QJsonObject &data);
    void finish(const QString &requestId, bool ok, const QVariantMap &answer, const QString &error);

    ProgramTransport *transport;
    QHash<QString, Pending> pending;
    int timeoutMs = 15000;
};

#endif // PROGRAMCLIENT_H
