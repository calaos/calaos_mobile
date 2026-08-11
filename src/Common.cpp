#include "Common.h"
#include "IOTypeRegistry.h"
#include <QStandardPaths>
#include <cmath>
#include <limits>

/* Both directions read the same table (see src/IOTypeRegistry.h), so a type
 * added there is parsed and serialized without touching this file. Note that
 * the returned string is the gui_type only: the styled types (Pump, DoorSensor
 * ...) share the string of the type they specialize and are told apart by
 * their style, which IOTypeFromString() takes as a second argument. */
QString Common::IOTypeToString(Common::IOType t)
{
    return IOTypeRegistry::guiType(t);
}

Common::IOType Common::IOTypeFromString(QString t, QString style)
{
    return IOTypeRegistry::fromGuiType(t, style);
}

QString Common::audioStatusToString(Common::AudioStatusType t)
{
    switch (t)
    {
    case StatusPause: return "pause";
    case StatusPlay: return "play";
    case StatusStop: return "stop";
    default: break;
    }

    return QString();
}

Common::AudioStatusType Common::audioStatusFromString(QString t)
{
    if (t == "play" || t == "playing") return StatusPlay;
    if (t == "pause") return StatusPause;
    if (t == "stop") return StatusStop;

    return StatusUnknown;
}

/* Single trace shared by every parsing helper: only one format to know when
 * grepping the logs for "which server value was unreadable". */
static void warnParseFailed(const char *kind, const char *what, const QString &raw)
{
    qWarning().noquote() << QStringLiteral("Common: cannot parse %1 as %2, got \"%3\", using default")
                            .arg(QString::fromLatin1(what ? what : "value"),
                                 QString::fromLatin1(kind),
                                 raw);
}

/* A key missing from a QVariantMap, or a field the server sent empty, is not
 * malformed data: return the default without polluting the logs. */
static bool isAbsentOrEmpty(const QVariant &v)
{
    if (!v.isValid() || v.isNull())
        return true;

    const int id = v.metaType().id();
    if (id == QMetaType::QString || id == QMetaType::QByteArray)
        return v.toString().isEmpty();

    return false;
}

int Common::toIntSafe(const QString &s, int def, const char *what)
{
    if (s.isEmpty())
        return def;

    bool ok = false;
    const int val = s.toInt(&ok);
    if (ok)
        return val;

    warnParseFailed("int", what, s);
    return def;
}

double Common::toDoubleSafe(const QString &s, double def, const char *what)
{
    if (s.isEmpty())
        return def;

    bool ok = false;
    const double val = s.toDouble(&ok);
    if (ok)
        return val;

    warnParseFailed("double", what, s);
    return def;
}

qint64 Common::toLongLongSafe(const QString &s, qint64 def, const char *what)
{
    if (s.isEmpty())
        return def;

    bool ok = false;
    const qint64 val = s.toLongLong(&ok);
    if (ok)
        return val;

    warnParseFailed("qint64", what, s);
    return def;
}

int Common::toIntSafe(const QVariant &v, int def, const char *what)
{
    if (isAbsentOrEmpty(v))
        return def;

    bool ok = false;
    const int val = v.toInt(&ok);
    if (ok)
        return val;

    warnParseFailed("int", what, v.toString());
    return def;
}

double Common::toDoubleSafe(const QVariant &v, double def, const char *what)
{
    if (isAbsentOrEmpty(v))
        return def;

    bool ok = false;
    const double val = v.toDouble(&ok);
    if (ok)
        return val;

    warnParseFailed("double", what, v.toString());
    return def;
}

qint64 Common::toLongLongSafe(const QVariant &v, qint64 def, const char *what)
{
    if (isAbsentOrEmpty(v))
        return def;

    bool ok = false;
    const qint64 val = v.toLongLong(&ok);
    if (ok)
        return val;

    warnParseFailed("qint64", what, v.toString());
    return def;
}

int Common::toIntSafe(const QJsonValue &v, int def, const char *what)
{
    if (v.isUndefined() || v.isNull())
        return def;

    if (v.isDouble())
    {
        const double d = v.toDouble();
        if (std::trunc(d) == d &&
            d >= static_cast<double>(std::numeric_limits<int>::min()) &&
            d <= static_cast<double>(std::numeric_limits<int>::max()))
            return static_cast<int>(d);

        warnParseFailed("int", what, QString::number(d));
        return def;
    }

    //some servers carry numbers as strings
    if (v.isString())
        return toIntSafe(v.toString(), def, what);

    warnParseFailed("int", what, v.toVariant().toString());
    return def;
}

double Common::toDoubleSafe(const QJsonValue &v, double def, const char *what)
{
    if (v.isUndefined() || v.isNull())
        return def;

    if (v.isDouble())
        return v.toDouble();

    if (v.isString())
        return toDoubleSafe(v.toString(), def, what);

    warnParseFailed("double", what, v.toVariant().toString());
    return def;
}

static void _messageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString fname = context.file;
    fname = fname.section('\\', -1, -1);

    switch (type) {
    default:
    case QtDebugMsg:
    {
        QString s = QString("DEBUG: %1:%2 - %3\n").arg(fname).arg(context.line).arg(msg);
        printf("%s", qPrintable(s));
        break;
    }
    case QtWarningMsg:
    {
        QString s = QString("WARNING: %1:%2 - %3\n").arg(fname).arg(context.line).arg(msg);
        printf("%s", qPrintable(s));
        break;
    }
    case QtCriticalMsg:
    {
        QString s = QString("CRITICAL: %1:%2 - %3\n").arg(fname).arg(context.line).arg(msg);
        printf("%s", qPrintable(s));
        break;
    }
    case QtFatalMsg:
    {
        QString s = QString("FATAL: %1:%2 - %3\n").arg(fname).arg(context.line).arg(msg);
        printf("%s", qPrintable(s));
        break;
    }
    }

    fflush(stdout);
}

void Common::installMessageOutputHandler()
{
    qInstallMessageHandler(_messageOutput);
}

QString Common::getDemoUser()
{
    return QStringLiteral("demo@calaos.fr");
}

QString Common::getDemoPass()
{
    return QStringLiteral("demo");
}

QString Common::getDemoHost()
{
    return QStringLiteral("demo.calaos.fr");
}
