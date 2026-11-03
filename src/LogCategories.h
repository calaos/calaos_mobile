#ifndef LOGCATEGORIES_H
#define LOGCATEGORIES_H

#include <QLoggingCategory>

//Header only so that small units can log without linking the rest. Both
//categories stay at warning level by default: a kiosk in steady state writes
//nothing. Enable the traces with QT_LOGGING_RULES="calaos.*.debug=true".
inline const QLoggingCategory &lcNet()
{
    static const QLoggingCategory category("calaos.net", QtWarningMsg);
    return category;
}

//Periodic or per-action traces that are not network traffic.
inline const QLoggingCategory &lcApp()
{
    static const QLoggingCategory category("calaos.app", QtWarningMsg);
    return category;
}

#endif
