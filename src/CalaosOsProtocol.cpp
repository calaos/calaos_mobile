#include "CalaosOsProtocol.h"

namespace CalaosOsProtocol {

Envelope parseEnvelope(const QJsonDocument &doc)
{
    Envelope env;
    if (!doc.isObject())
        return env;

    const QJsonObject o = doc.object();
    if (!o.value(QStringLiteral("error")).isBool())
        return env;

    env.error = o.value(QStringLiteral("error")).toBool();
    env.code = o.value(QStringLiteral("code")).toString();
    env.msg = o.value(QStringLiteral("msg")).toString();
    env.data = o.value(QStringLiteral("data"));

    //legacy bodies also carry error/msg ({error,msg,output}, {error,msg}):
    //an error envelope always has a code, a success one has data
    env.isEnvelope = env.error ? !env.code.isEmpty() : o.contains(QStringLiteral("data"));
    return env;
}

bool isLegacyDaemonReply(int httpStatus, const QJsonDocument &doc)
{
    if (httpStatus != 404)
        return false;
    const Envelope env = parseEnvelope(doc);
    return !(env.isEnvelope && env.error);
}

bool isUpgradeRouteMissing(int httpStatus, const Envelope &env)
{
    return httpStatus == 404 && env.isEnvelope && env.code == QStringLiteral("not_found");
}

bool isUpgradeInProgress(ApiVersion api, int httpStatus, const Envelope &env, const QString &legacyMsg)
{
    if (api == ApiVersion::V2)
        return httpStatus == 409 && env.code == QStringLiteral("upgrade_in_progress");
    //legacy routes use 409 for that only (system/reboot during a run)
    return httpStatus == 409 || legacyMsg.contains(QStringLiteral("in progress"), Qt::CaseInsensitive);
}

bool isPackageNotFound(ApiVersion api, int httpStatus, const Envelope &env, const QString &legacyMsg)
{
    if (api == ApiVersion::V2)
        return httpStatus == 404 && env.code == QStringLiteral("package_not_found");
    return legacyMsg.contains(QStringLiteral("package not found"), Qt::CaseInsensitive);
}

bool isIdleProgress(const QJsonObject &progress)
{
    const QJsonValue v = progress.value(QStringLiteral("status"));
    if (!v.isString())
        return false;
    const QString s = v.toString();
    return s == QStringLiteral("idle") || s.isEmpty();
}

QString authorizationHeader(const QString &token)
{
    return QStringLiteral("Bearer ") + token;
}

bool Job::isTerminal() const
{
    return status == QStringLiteral("done") || status == QStringLiteral("error");
}

bool Job::succeeded() const
{
    return status == QStringLiteral("done");
}

Job parseJob(const QJsonValue &v)
{
    Job j;
    if (!v.isObject())
        return j;

    const QJsonObject o = v.toObject();
    j.id = o.value(QStringLiteral("id")).toString();
    j.kind = o.value(QStringLiteral("kind")).toString();
    j.package = o.value(QStringLiteral("package")).toString();
    j.status = o.value(QStringLiteral("status")).toString();
    j.error = o.value(QStringLiteral("error")).toString();
    j.rebootRequired = o.value(QStringLiteral("reboot_required")).toBool();
    j.daemonRestart = o.value(QStringLiteral("daemon_restart")).toBool();
    j.valid = !j.id.isEmpty() && !j.status.isEmpty();
    return j;
}

UpdateStatus parseUpdateStatus(ApiVersion api, const QJsonValue &payload)
{
    UpdateStatus st;
    const QJsonObject o = payload.toObject();

    if (api != ApiVersion::V2)
    {
        st.progress = o;
        return st;
    }

    st.progress = o.value(QStringLiteral("progress")).toObject();
    st.hasJobField = o.contains(QStringLiteral("job"));
    st.job = parseJob(o.value(QStringLiteral("job")));
    return st;
}

void UpgradeJobTracker::clear()
{
    m_id.clear();
    m_pending = false;
    m_finished = Job();
    m_early.clear();
}

void UpgradeJobTracker::startPending()
{
    m_id.clear();
    m_pending = true;
    m_finished = Job();
    m_early.clear();
}

UpgradeJobTracker::Action UpgradeJobTracker::follow(const QString &id)
{
    const Job early = m_early.value(id);
    m_early.clear();
    m_pending = false;
    m_id = id;

    if (early.valid && early.isTerminal())
    {
        m_finished = early;
        m_id.clear();
        return Action::Finish;
    }
    return Action::None;
}

UpgradeJobTracker::Action UpgradeJobTracker::onEvent(const Job &job, bool adoptedWithoutId)
{
    if (!job.valid)
        return Action::None;

    if (m_pending)
    {
        //bounded: only a handful of jobs can start while one POST is in flight
        if (m_early.size() >= 8)
            m_early.clear();
        m_early.insert(job.id, job);
        return Action::None;
    }

    if (isFollowing())
    {
        if (job.id != m_id || !job.isTerminal())
            return Action::None;
        m_finished = job;
        m_id.clear();
        return Action::Finish;
    }

    if (job.status == QStringLiteral("running"))
    {
        m_id = job.id;
        return Action::Adopt;
    }

    //a live terminal event while we show a run we adopted without its id:
    //only one run exists at a time, this is its end
    if (adoptedWithoutId && job.isTerminal())
    {
        m_finished = job;
        return Action::Finish;
    }

    return Action::None;
}

UpgradeJobTracker::Action UpgradeJobTracker::onStatus(const Job &job, bool hasJobField)
{
    if (m_pending || !hasJobField)
        return Action::None;

    if (isFollowing())
    {
        //update/status returns the running job or the last one: any other
        //answer means ours is gone and its outcome is unknown
        if (!job.valid || job.id != m_id)
        {
            m_id.clear();
            return Action::Lost;
        }
        if (!job.isTerminal())
            return Action::None;
        m_finished = job;
        m_id.clear();
        return Action::Finish;
    }

    //a terminal job here may be an older run: only a running one is adopted
    if (job.valid && job.status == QStringLiteral("running"))
    {
        m_id = job.id;
        return Action::Adopt;
    }

    return Action::None;
}

} // namespace CalaosOsProtocol
