#include "WeatherInfo.h"
#include "HardwareUtils.h"
#include <QQmlEngine>
#include <QtQml>

#define OWM_KEY         "29497da09cebc741eec3adfc9969ca51"
#define KELVIN_ZERO     273.15

static void forecastAppend(QQmlListProperty<WeatherData> *prop, WeatherData *val)
{
    Q_UNUSED(val);
    Q_UNUSED(prop);
}

static WeatherData *forecastAt(QQmlListProperty<WeatherData> *prop, int index)
{
    WeatherModel *m = static_cast<WeatherModel*>(prop->data);
    return m->getForecastData(index);
}

static int forecastCount(QQmlListProperty<WeatherData> *prop)
{
    WeatherModel *m = static_cast<WeatherModel*>(prop->data);
    return m->getForecastCount();
}

static void forecastClear(QQmlListProperty<WeatherData> *prop)
{
    static_cast<WeatherModel*>(prop->data)->forecastDataClear();
}

WeatherModel::WeatherModel(QObject *parent):
    QObject(parent)
{
    accessManager = new QNetworkAccessManager(this);
    qdataForecast = new QQmlListProperty<WeatherData>(this,
                                                      this,
                                                      forecastAppend,
                                                      forecastCount,
                                                      forecastAt,
                                                      forecastClear);
    QQmlEngine::setObjectOwnership(&dataNow, QQmlEngine::CppOwnership);
    update_weather(&dataNow);
}

WeatherModel::~WeatherModel()
{
    for (int i = 0;i < dataForecast.count();i++)
        delete dataForecast.at(i);
    dataForecast.clear();
}

void WeatherModel::refreshWeather()
{
    QUrl url("http://api.openweathermap.org/data/2.5/weather");
    QUrlQuery query;
    query.addQueryItem("lat", HardwareUtils::Instance()->getConfigOption("latitude"));
    query.addQueryItem("lon", HardwareUtils::Instance()->getConfigOption("longitude"));
    query.addQueryItem("mode", "json");
    query.addQueryItem("APPID", OWM_KEY);
    url.setQuery(query);

    QNetworkReply *rep = accessManager->get(QNetworkRequest(url));

    connect(rep, SIGNAL(finished()),
            this, SLOT(handleWeatherNetworkData()));
}

void WeatherModel::handleWeatherNetworkData()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;

    if (!reply->error())
    {
        QJsonDocument jdoc = QJsonDocument::fromJson(reply->readAll());

        qDebug().noquote() << "RECV: " << jdoc.toJson();

        if (jdoc.isObject())
        {
            QJsonObject obj = jdoc.object();
            dataNow.setWeatherData(obj);
        }
    }
    else
    {
        qWarning() << "Error in weather request " << reply->url() << ": " << reply->errorString();
    }

    QUrl url("http://api.openweathermap.org/data/2.5/forecast/daily");
    QUrlQuery query;
    query.addQueryItem("lat", HardwareUtils::Instance()->getConfigOption("latitude"));
    query.addQueryItem("lon", HardwareUtils::Instance()->getConfigOption("longitude"));
    query.addQueryItem("mode", "json");
    query.addQueryItem("cnt", "5");
    query.addQueryItem("APPID", OWM_KEY);
    url.setQuery(query);

    QNetworkReply *rep = accessManager->get(QNetworkRequest(url));

    connect(rep, SIGNAL(finished()),
            this, SLOT(handleForecastNetworkData()));
}

void WeatherModel::handleForecastNetworkData()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;

    if (!reply->error())
    {
        QJsonDocument jdoc = QJsonDocument::fromJson(reply->readAll());

        qDebug().noquote() << "RECV: " << jdoc.toJson();

        for (int i = 0;i < dataForecast.count();i++)
            delete dataForecast.at(i);
        dataForecast.clear();

        if (jdoc.isObject())
        {
            QJsonObject obj = jdoc.object();
            QJsonArray jfc = obj["list"].toArray();
            for (int i = 0;i < jfc.count();i++)
            {
                WeatherData *wd = new WeatherData;
                wd->setWeatherData(jfc.at(i).toObject());
                dataForecast.append(wd);
            }
        }
    }
    else
    {
        qWarning() << "Error in weather request " << reply->url() << ": " << reply->errorString();
    }
}

WeatherData::WeatherData(const WeatherData &other):
    QObject(nullptr),
    m_dayOfWeek(other.m_dayOfWeek),
    m_weatherIcon(other.m_weatherIcon),
    m_weatherCode(other.m_weatherCode),
    m_weatherDescription(other.m_weatherDescription),
    m_weatherText(other.m_weatherText),
    m_temperature(other.m_temperature),
    m_temperatureMin(other.m_temperatureMin),
    m_temperatureMax(other.m_temperatureMax),
    m_pressure(other.m_pressure),
    m_humidity(other.m_humidity),
    m_isNight(other.m_isNight)
{
}

WeatherData::WeatherData():
    QObject(nullptr)
{
    update_dayOfWeek("");
    update_humidity("--");
    update_isNight(false);
    update_pressure("--");
    update_temperature("--");
    update_temperatureMax("--");
    update_temperatureMin("--");
    update_weatherCode(0);
    update_weatherDescription("--");
    update_weatherIcon("");
    update_weatherText("--");

//    //Sample data
//    update_dayOfWeek("Lundi");
//    update_humidity("44");
//    update_isNight(false);
//    update_pressure("1013");
//    update_temperature("19");
//    update_temperatureMax("33");
//    update_temperatureMin("-4");
//    update_weatherCode(503);
//    update_weatherDescription("light intensity shower rain");
//    update_weatherIcon("09d");
//    update_weatherText("Rain");
}

QString WeatherData::convertTemp(double t)
{
    return QString::number(qRound(t - KELVIN_ZERO));
}

void WeatherData::setWeatherData(const QJsonObject &obj)
{
    QDateTime dt = QDateTime::fromMSecsSinceEpoch((quint64)obj["dt"].toDouble() * 1000);
    update_dayOfWeek(dt.date().toString("ddd"));

    QJsonArray jarr = obj["weather"].toArray();
    if (jarr.count() > 0)
    {
        QJsonObject jw = jarr.at(0).toObject();
        update_weatherIcon(jw["icon"].toString());
        update_weatherCode(jw["id"].toInt());
        update_weatherText(jw["main"].toString());
        update_weatherDescription(jw["description"].toString());
    }

    QJsonObject tobj = obj["main"].toObject();
    update_temperature(convertTemp(tobj["temp"].toDouble()));
    update_temperatureMin(convertTemp(tobj["temp_min"].toDouble()));
    update_temperatureMax(convertTemp(tobj["temp_max"].toDouble()));
    update_pressure(tobj["pressure"].toString());
    update_humidity(tobj["humidity"].toString());

    update_isNight(get_weatherIcon().endsWith('n'));
}

WeatherData *WeatherModel::getForecastData(int i)
{
    WeatherData *w = dataForecast.at(i);
    QQmlEngine::setObjectOwnership(w, QQmlEngine::CppOwnership);
    return w;
}

int WeatherModel::getForecastCount()
{
    return dataForecast.count();
}

void WeatherModel::forecastDataClear()
{
    dataForecast.clear();
}

QQmlListProperty<WeatherData> WeatherModel::getForecast()
{
    return *qdataForecast;
}

void WeatherModel::registerQmlClasses()
{
    qmlRegisterType<WeatherData>("WeatherInfo", 1, 0, "WeatherData");
    qmlRegisterType<WeatherModel>("WeatherInfo", 1, 0, "WeatherModel");
    qRegisterMetaType<WeatherData>();
}
