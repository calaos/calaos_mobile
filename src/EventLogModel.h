#ifndef EVENTLOGMODEL_H
#define EVENTLOGMODEL_H

#include <QStandardItemModel>
#include <QSet>
#include <QTimer>
#include "qqmlhelpers.h"
#include <QQmlApplicationEngine>
#include "CalaosConnection.h"
#include "Common.h"

namespace EventLogIO
{
    // Resout un identifiant d'IO en cherchant d'abord cote entree, puis cote
    // sortie (ordre historique). Extrait de EventLogItem::load() en fonction
    // pure parametree par les callbacks de recherche, pour rester testable
    // (tests/tst_eventlogmodel) sans lier IOCache/IOBase (RoomModel.cpp), qui
    // entrainent CalaosConnection, HardwareUtils et quickflux dans le lien.
    template <typename T, typename SearchInFn, typename SearchOutFn>
    T *resolve(SearchInFn searchInput, SearchOutFn searchOutput, const QString &id)
    {
        T *io = searchInput(id);
        if (!io)
            io = searchOutput(id);
        return io;
    }
}

#define DEFAULT_PER_PAGE    50

class EventLogItem;

//Pages of the event log, newest first (EVLOG-1).
//
//At most one page request is in flight. It always ends: with its answer, with
//the server's error, with the transport going away (CalaosConnection fails
//every request still pending), with the connection leaving the connected
//state, or with a guard timeout for a server that never answers. Its id ties
//the answer to it, so a late answer to an abandoned request is ignored.
//refresh() always starts over with page 0 and replaces the rows once that
//page arrives; loadMore() appends the next page and stops at the last one.
//
//Identical consecutive events are one row carrying evCount, also across two
//pages. Paging and shownIds still count the events the server sent.
class EventLogModel : public QStandardItemModel
{
    Q_OBJECT
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)
    Q_PROPERTY(bool loadingMore READ isLoadingMore NOTIFY loadingChanged)
    Q_PROPERTY(bool error READ hasError NOTIFY errorChanged)
public:
    explicit EventLogModel(QQmlApplicationEngine *engine, CalaosConnection *con, QObject *parent = nullptr);
    virtual ~EventLogModel() {}

    //How long a request (a page, or the single event of loadEvent()) may
    //stay unanswered before it is abandoned.
    static constexpr int DefaultRequestTimeoutMs = 25000;

    enum
    {
        RoleTitle = Qt::UserRole + 1,
        RoleDate,
        RoleTime,
        RoleType,
        RoleIconSource,
        RoleRoomName,
        RoleNotifText,
        RoleNotifHasPic,
        RoleNotifPicUrl,
        RoleActionText,
        RoleCount,
    };

    Q_INVOKABLE QObject *getEventModel(int idx) const;

    //Appends the next page. Nothing while a page is in flight or once the
    //last page is shown.
    Q_INVOKABLE void loadMore();
    //Reloads from the newest event. Always sends a new request (abandoning
    //the one in flight, if any); the rows are replaced when it is answered.
    Q_INVOKABLE void refresh();

    Q_INVOKABLE QObject *loadEvent(QString uuid);

    //Forgets everything: rows, paging and the request in flight. Called by
    //Application on logout (possibly before logging in to another home) and
    //when the credentials were given up on (loginFailed()).
    void reset();

    bool isLoading() const { return !pendingId.isEmpty(); }
    //A page to append, not a refresh: the rows below it stay as they are.
    bool isLoadingMore() const { return isLoading() && !pendingReplace; }
    bool isAtEnd() const { return atEnd; }
    //The last page asked for did not arrive. The next request clears it.
    bool hasError() const { return loadFailed; }

    //Tests use a short one.
    void setRequestTimeout(int ms) { requestTimeoutMs = ms; }
    int requestTimeout() const { return requestTimeoutMs; }

signals:
    void loadingChanged();
    void errorChanged();

private slots:
    void pageFinished(const QString &requestId, bool ok, const QVariantMap &data);
    void connectionStateChanged(int state);
    void requestTimedOut();

private:
    void requestPage(int page, bool replace);
    //Stops waiting for the request in flight; its answer will be ignored.
    void abandonRequest();
    void appendEvents(const QVariantList &events);
    void setLoadFailed(bool failed);
    //Tells QML once an operation is over, not at each step of it: refresh()
    //must not look idle between dropping a request and sending the next, nor
    //a page look done before its rows are in.
    void publishLoading();

    QQmlApplicationEngine *engine;
    CalaosConnection *connection;

    bool loadFailed = false;
    bool shownLoading = false;
    bool shownLoadingMore = false;

    //Request in flight: its id (empty when none), the page it asks for and
    //whether its answer replaces the rows (refresh) or is appended.
    QString pendingId;
    int pendingPage = 0;
    bool pendingReplace = false;
    QTimer requestTimer;
    int requestTimeoutMs = DefaultRequestTimeoutMs;

    //Paging, tracked from the pages actually received rather than derived
    //from rowCount(): a last page shorter than DEFAULT_PER_PAGE used to be
    //requested again and its rows duplicated.
    int nextPage = 0;
    bool atEnd = false;
    //Events shown, by their "id" (the event uuid), so a page that overlaps
    //the previous one (new events logged in between shift the pages) adds
    //no duplicate row.
    QSet<QString> shownIds;
};

class EventLogItem: public QObject, public QStandardItem
{
    Q_OBJECT

    QML_READONLY_PROPERTY_MODEL(QString, evTitle, EventLogModel::RoleTitle)
    QML_READONLY_PROPERTY_MODEL(QString, evDate, EventLogModel::RoleDate)
    QML_READONLY_PROPERTY_MODEL(QString, evTime, EventLogModel::RoleTime)
    QML_READONLY_PROPERTY_MODEL(Common::EventType, evType, EventLogModel::RoleType)
    QML_READONLY_PROPERTY_MODEL(QString, evIconSource, EventLogModel::RoleIconSource)
    QML_READONLY_PROPERTY_MODEL(QString, evRoomName, EventLogModel::RoleRoomName)
    QML_READONLY_PROPERTY_MODEL(QString, evNotifText, EventLogModel::RoleNotifText)
    QML_READONLY_PROPERTY_MODEL(bool, evHasPicture, EventLogModel::RoleNotifHasPic)
    QML_READONLY_PROPERTY_MODEL(QString, evPictureUrl, EventLogModel::RoleNotifPicUrl)
    QML_READONLY_PROPERTY_MODEL(QString, evActionText, EventLogModel::RoleActionText)
    QML_READONLY_PROPERTY_MODEL(int, evCount, EventLogModel::RoleCount)

    QML_READONLY_PROPERTY(bool, loading)

public:
    EventLogItem(QQmlApplicationEngine *engine, CalaosConnection *con);
    virtual ~EventLogItem() {}

    void load(const QVariantMap &data);
    //Whether the two read the same in the list, whatever their time. A
    //notification with a picture stays alone: each one opens its own picture.
    bool isSameEvent(const EventLogItem &other) const;
    //Asks the server for that one event. loading stays true until it is
    //answered, the session drops, or timeoutMs passes without an answer
    //(a silent server must not leave the push view spinning forever).
    void load(const QString &uuid, int timeoutMs = EventLogModel::DefaultRequestTimeoutMs);

private slots:
    void eventFinished(const QString &requestId, bool ok, const QVariantMap &data);

private:
    void endRequest();

    QQmlApplicationEngine *engine;
    CalaosConnection *connection;
    QString requestId;
    //Only created for the items that load an event themselves, not for the
    //rows of the model.
    QTimer *requestTimer = nullptr;
};

#endif // EVENTLOGMODEL_H
