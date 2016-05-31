#ifndef WEATHERINFO_H
#define WEATHERINFO_H

#include <QtCore>
#include <QQmlListProperty>
#include <QtNetwork>
#include "qqmlhelpers.h"

/* Classes for exposing weather info from Openweathermap to QML
*/

class WeatherData: public QObject
{
    Q_OBJECT

    QML_READONLY_PROPERTY(QString, dayOfWeek)
    QML_READONLY_PROPERTY(QString, weatherIcon)
    QML_READONLY_PROPERTY(int, weatherCode)
    QML_READONLY_PROPERTY(QString, weatherDescription)
    QML_READONLY_PROPERTY(QString, weatherText)
    QML_READONLY_PROPERTY(QString, temperature)
    QML_READONLY_PROPERTY(QString, temperatureMin)
    QML_READONLY_PROPERTY(QString, temperatureMax)
    QML_READONLY_PROPERTY(QString, pressure)
    QML_READONLY_PROPERTY(QString, humidity)
    QML_READONLY_PROPERTY(bool, isNight)
public:
    WeatherData();
    WeatherData(const WeatherData &other);

    void setWeatherData(const QJsonObject &obj);

private:
    QString convertTemp(double t);
};
Q_DECLARE_METATYPE(WeatherData)

class WeatherModel: public QObject
{
    Q_OBJECT

    QML_READONLY_PROPERTY(WeatherData *, weather)
    Q_PROPERTY(QQmlListProperty<WeatherData> forecast READ getForecast NOTIFY forecastChanged)
public:
    WeatherModel(QObject *parent = 0);
    virtual ~WeatherModel();

    Q_INVOKABLE void refreshWeather();

    QQmlListProperty<WeatherData> getForecast();
    WeatherData *getForecastData(int i);
    int getForecastCount();
    void forecastDataClear();

    static void registerQmlClasses();

signals:
    void forecastChanged();

private slots:
    void handleWeatherNetworkData();
    void handleForecastNetworkData();

private:
    QNetworkAccessManager *accessManager;

    WeatherData dataNow;
    QList<WeatherData *> dataForecast;
    QQmlListProperty<WeatherData> *qdataForecast;

    //time info to prevent querying server too fast
    QTime refreshTime;
};

#endif // WEATHERINFO_H
