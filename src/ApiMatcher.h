#ifndef APIMATCHER_H
#define APIMATCHER_H

#include <QObject>

class ApiMatcher : public QObject
{
    Q_OBJECT
public:
    explicit ApiMatcher(QObject *parent = 0);
    virtual ~ApiMatcher();

public slots:
    void processJson(const QVariantMap &vmap);
};

#endif // APIMATCHER_H
