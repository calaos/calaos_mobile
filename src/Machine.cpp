#include "Machine.h"

NetworkInfo::NetworkInfo(QObject *parent):
    QObject(parent)
{
    update_isLoopback(false);
}
