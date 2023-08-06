#include "AsyncJobs.h"
#include <QVariant>

void AsyncJob::start(const QVariant &previous_data)
{
    jobFunc(this, previous_data);
}

AsyncJobs::AsyncJobs(QObject *parent):
    QObject(parent)
{
}

AsyncJobs::~AsyncJobs()
{
}

void AsyncJobs::append(AsyncJob *j)
{
    if (!j) return;

    //reparent object to handle jobs memory from AsyncJobs
    j->setParent(this);
    jobs.enqueue(j);
}

void AsyncJobs::prepend(AsyncJob *j)
{
    if (!j) return;

    //reparent object to handle jobs memory from AsyncJobs
    j->setParent(this);
    jobs.prepend(j);
}

void AsyncJobs::start()
{
    if (running) return;
    running = true;

    //start running or queued jobs if any
    dequeueStartJob({});
}

void AsyncJobs::dequeueStartJob(const QVariant &data)
{
    if (jobs.isEmpty())
    {
        //end of job queue, emit finished signal
        //and delete job runner
        emit finished(data);
        deleteLater();
        return;
    }

    currentJob = jobs.dequeue();
    connect(currentJob, SIGNAL(done(QVariant)), this, SLOT(jobDone(QVariant)));
    connect(currentJob, SIGNAL(error()), this, SLOT(jobFailed()));
    currentJob->start(data);
}

void AsyncJobs::jobFailed()
{
    emit failed(currentJob);
    deleteLater();
}

void AsyncJobs::jobDone(const QVariant &data)
{
    dequeueStartJob(data);
}
