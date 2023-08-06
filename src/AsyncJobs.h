#ifndef ASYNCJOBS_H
#define ASYNCJOBS_H

#include <QObject>
#include <QByteArray>
#include <QQueue>
#include <functional>
#include <QVariant>

/*
 * Classes for running jobs
 * It handles chaining the jobs to start the next one after one has completed successfuly.
 * It also handles failure: when a job fails, the job queue is stopped.
 *
 * The AsyncJobs manages the job queue and it autodeletes itself when finished for convenience.
 *
 * AsyncJobs queue support adding more jobs to the queue dynamically (even from the callback from
 * one running job). This is usefull to add jobs to the queue that are different based on the result
 * of the data received from the previous job.
 */

class AsyncJob;

typedef std::function<void(AsyncJob *job, const QVariant &data)> AsyncFunc;

class AsyncJob: public QObject
{
    Q_OBJECT
public:
    AsyncJob(AsyncFunc jobfn):
        jobFunc(std::move(jobfn))
    {}

public slots:
    virtual void start(const QVariant &previous_data);

    virtual void emitSuccess(const QVariant &data = {}) { emit done(data); }
    virtual void emitFailed() { emit error(); }

signals:
    void done(const QVariant &data);
    void error();

protected:
    //callback with data from previous Job
    AsyncFunc jobFunc = [](AsyncJob *, const QVariant &) {};
};

class AsyncJobs: public QObject
{
    Q_OBJECT
public:
    AsyncJobs(QObject *parent = nullptr);
    virtual ~AsyncJobs();

    void append(AsyncJob *j);
    void prepend(AsyncJob *j);

public slots:
    void start();

signals:
    void finished(const QVariant &data);
    void failed(AsyncJob *job);

private slots:
    void dequeueStartJob(const QVariant &data);
    void jobDone(const QVariant &data);
    void jobFailed();

private:
    QQueue<AsyncJob *> jobs;
    bool running = false;
    AsyncJob *currentJob = nullptr;
};

#endif // ASYNCJOBS_H
