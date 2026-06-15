#ifndef WEATHERMANAGER_H
#define WEATHERMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>

class WeatherManager : public QObject
{
    Q_OBJECT
public:
    static WeatherManager *instance();

    WeatherManager(const WeatherManager &)            = delete;
    WeatherManager &operator=(const WeatherManager &) = delete;

    // ========== 天气相关接口 ==========
    void requestWeather();                                  // 自动定位并请求天气
    void requestWeather(double latitude, double longitude); // 使用指定经纬度请求天气
    void requestWeather(const QString &cityName);           // 使用指定城市名请求天气（暂不支持）

    // ========== 地图导航接口 ==========

signals:
    // 天气信号
    void weatherDataReady(const QString &city, int temp, const QString &text);
    void forecastReady(const QStringList &dates,
                       const QList<int> &highs,
                       const QList<int> &lows,
                       const QStringList &texts);
    void errorOccurred(const QString &error);

private slots:
    void onIpLocationFinished();
    void onBaiduWeatherFinished();

private:
    explicit WeatherManager(QObject *parent = nullptr);
    ~WeatherManager() override = default;

    // 内部请求函数
    void fetchWeatherByLatLon(double lat, double lon);

    QNetworkAccessManager m_nam;
    static const QString BAIDU_AK;
};

#endif // WEATHERMANAGER_H
