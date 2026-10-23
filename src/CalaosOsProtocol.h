#ifndef CALAOSOSPROTOCOL_H
#define CALAOSOSPROTOCOL_H

#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QHash>

/*
 * Wire rules of the calaos-container daemon API (desktop only), kept free of
 * any network code so they can be unit tested.
 *
 * The daemon serves two surfaces: the versioned /api/v2 (one JSON envelope
 * {error, code, msg, data}, error codes, asynchronous upgrade jobs) and the
 * frozen unprefixed legacy routes. A daemon that predates /api/v2 answers a
 * plain-text 404 on every /api/v2 route: that, and only that, sends us back
 * to the legacy routes.
 */
namespace CalaosOsProtocol {

enum class ApiVersion
{
    Unknown, //not probed yet, or the daemon went away since
    Legacy,  //daemon without /api/v2
    V2,
};

struct Envelope
{
    bool isEnvelope = false; //the body is a /api/v2 envelope
    bool error = false;
    QString code;            //set on errors only
    QString msg;
    QJsonValue data;
};

Envelope parseEnvelope(const QJsonDocument &doc);

//A /api/v2 answer that means "this daemon has no /api/v2": a 404 whose body
//is not an error envelope carrying a code. A 404 with a code (not_found,
//package_not_found) comes from a daemon that does serve /api/v2.
bool isLegacyDaemonReply(int httpStatus, const QJsonDocument &doc);

//A daemon serving /api/v2 but predating upgrade jobs answers not_found on the
//upgrade routes: the upgrade then runs on the legacy synchronous route.
bool isUpgradeRouteMissing(int httpStatus, const Envelope &env);

//Legacy daemons only say it in the message; /api/v2 has dedicated codes.
bool isUpgradeInProgress(ApiVersion api, int httpStatus, const Envelope &env, const QString &legacyMsg);
bool isPackageNotFound(ApiVersion api, int httpStatus, const Envelope &env, const QString &legacyMsg);

//Daemons 1.5.0 to 1.7.0 report an empty status at rest instead of "idle".
bool isIdleProgress(const QJsonObject &progress);

QString authorizationHeader(const QString &token);

struct Job
{
    bool valid = false;
    QString id;
    QString kind;
    QString package;
    QString status; //"running", "done" or "error"
    QString error;
    bool rebootRequired = false;
    bool daemonRestart = false;

    bool isTerminal() const;
    bool succeeded() const;
};

Job parseJob(const QJsonValue &v);

//GET update/status, both surfaces: legacy answers the raw progress object,
//v2 answers {"progress": <Status>, "job": <Job> or null}.
struct UpdateStatus
{
    QJsonObject progress;
    Job job;
    bool hasJobField = false; //the daemon runs upgrade jobs (even if none ran yet)
};

UpdateStatus parseUpdateStatus(ApiVersion api, const QJsonValue &payload);

/*
 * Which upgrade job we follow and when it ends. Only the terminal state of
 * that job ends the upgrade: a 2xx on the POST only means it was accepted.
 * Events are best effort and the daemon restarts after upgrading itself, so
 * the same job is also read from update/status, which is authoritative.
 */
class UpgradeJobTracker
{
public:
    enum class Action
    {
        None,
        Adopt,  //a job runs that we did not start: follow it
        Finish, //the job we follow has ended, see finishedJob()
        Lost,   //update/status no longer knows the job we follow
    };

    void clear();
    //an upgrade POST is in flight: its job id is not known yet
    void startPending();
    //the POST answered 202 (or 409 with the running job id)
    Action follow(const QString &id);

    //upgrade_job event. adoptedWithoutId: we show a run adopted from the
    //progress stream whose job id we do not know yet
    Action onEvent(const Job &job, bool adoptedWithoutId);
    //job of a GET update/status (job.valid false when it is null)
    Action onStatus(const Job &job, bool hasJobField);

    bool isPending() const { return m_pending; }
    bool isFollowing() const { return !m_id.isEmpty(); }
    const QString &jobId() const { return m_id; }
    const Job &finishedJob() const { return m_finished; }

private:
    QString m_id;
    bool m_pending = false;
    Job m_finished;
    //upgrade_job events seen while the POST was in flight: the terminal one
    //may beat the 202 answer
    QHash<QString, Job> m_early;
};

} // namespace CalaosOsProtocol

#endif // CALAOSOSPROTOCOL_H
