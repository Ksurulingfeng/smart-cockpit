#include "weathermanager.h"
#include "config.h"
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

const QString WeatherManager::BAIDU_AK = Config::BAIDU_AK;

WeatherManager *WeatherManager::instance()
{
    static WeatherManager instance;
    return &instance;
}

WeatherManager::WeatherManager(QObject *parent)
    : QObject(parent)
{
}

void WeatherManager::requestWeather()
{
    // 固定广州，不走 IP 定位
    fetchWeatherByLatLon(23.1052, 113.2762);
}

void WeatherManager::requestWeather(double latitude, double longitude)
{
    fetchWeatherByLatLon(latitude, longitude);
}

void WeatherManager::requestWeather(const QString &cityName)
{
    Q_UNUSED(cityName)
    emit errorOccurred("直接通过城市名查询天气暂未实现，请使用经纬度");
}

void WeatherManager::onIpLocationFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "[天气] IP定位失败:" << reply->errorString() << "→ 使用默认广州";
        reply->deleteLater();
        fetchWeatherByLatLon(23.1052, 113.2762);
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || doc.object()["status"].toString() != "success") {
        qDebug() << "[天气] IP定位JSON无效 → 使用默认广州";
        fetchWeatherByLatLon(23.1052, 113.2762);
        return;
    }

    QJsonObject obj = doc.object();
    double lat      = obj["lat"].toDouble();
    double lon      = obj["lon"].toDouble();
    QString city    = obj["city"].toString();
    qDebug() << "[天气] IP定位成功:" << city << lat << lon;
    fetchWeatherByLatLon(lat, lon);
}

void WeatherManager::fetchWeatherByLatLon(double lat, double lon)
{
    // 百度天气 API 需要经度,纬度顺序
    QString location = QString("%1,%2").arg(lon).arg(lat);
    QString url      = QString("http://api.map.baidu.com/weather/v1/"
                                    "?location=%1&data_type=all&ak=%2")
                      .arg(location, BAIDU_AK);
    QNetworkReply *reply = m_nam.get(QNetworkRequest(QUrl(url)));
    connect(reply, &QNetworkReply::finished, this,
            &WeatherManager::onBaiduWeatherFinished);
}

void WeatherManager::onBaiduWeatherFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;

    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred(
            QString("百度天气请求失败: %1").arg(reply->errorString()));
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        emit errorOccurred("百度天气 JSON 解析失败");
        return;
    }

    QJsonObject root = doc.object();
    if (root["status"].toInt() != 0) {
        emit errorOccurred(
            QString("百度天气 API 错误: %1").arg(root["message"].toString()));
        return;
    }

    QJsonObject result   = root["result"].toObject();
    QString city         = result["location"].toObject()["city"].toString();

    QJsonObject now      = result["now"].toObject();
    int temp             = now["temp"].toInt();
    QString text         = now["text"].toString();

    QJsonArray forecasts = result["forecasts"].toArray();
    QStringList dates, texts;
    QList<int> highs, lows;

    for (int i = 0; i < qMin(3, forecasts.size()); ++i) {
        QJsonObject day = forecasts[i].toObject();
        QString date    = day["date"].toString();
        dates.append(date.mid(5)); // "MM-DD"
        highs.append(day["high"].toInt());
        lows.append(day["low"].toInt());
        texts.append(day["text_day"].toString());
    }

    emit weatherDataReady(city, temp, text);
    if (!dates.isEmpty())
        emit forecastReady(dates, highs, lows, texts);
}
