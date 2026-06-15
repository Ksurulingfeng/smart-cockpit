#ifndef MAPMANAGER_H
#define MAPMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QStringList>
#include <QTimer>

class MapManager : public QObject
{
    Q_OBJECT

public:
    enum State { STATE_LOCATING,
                 STATE_NAVIGATING };

    explicit MapManager(QObject *parent = nullptr);
    ~MapManager() override;

    // GPS坐标 → 百度BD09坐标（本地计算，不消耗API配额）
    void requestCoordConv(double lat, double lon, bool immediate = false);
    static void wgs84ToBd09(double wgsLat, double wgsLon, double &bdLat, double &bdLon);
    // 地址 → 经纬度
    void requestGeocoding(const QString &address, const QString &tag);
    // 地点搜索建议
    void requestPlaceSuggestion(const QString &keyword, const QString &region = QString());
    // 路径规划
    void requestRoutePlan(const QString &originLatLon, const QString &destLatLon);
    // 静态图（定位模式，带标记点）
    void requestStaticMapForCoords(double bdLat, double bdLon, bool immediate = false);
    // 静态图（导航模式，带路径）
    void requestStaticMapWithPath(bool immediate = false);

    // 状态机
    void setState(State s);
    State state() const { return m_state; }

    // 路径点（用于导航地图）
    QString pathPoints() const { return m_pathPoints; }
    QString startLatLon() const { return m_startLatLon; }

    // 地图中心与缩放
    double centerLat() const { return m_centerLat; }
    double centerLon() const { return m_centerLon; }
    void setCenter(double lat, double lon);
    int zoom() const { return m_zoom; }
    void setZoom(int z);
    void zoomIn();
    void zoomOut();

    // 重置视图
    void resetView();

    // 计算像素偏移 → 经纬度变化
    void panByPixels(int dx, int dy, int viewWidth, int viewHeight);

    // GPS 位置
    double gpsLat() const { return m_gpsLat; }
    double gpsLon() const { return m_gpsLon; }
    void setGpsPosition(double lat, double lon);
    bool isGpsInitialized() const { return m_gpsInitialized; }

signals:
    void coordConverted(double bdLat, double bdLon);
    void geocodingDone(const QString &tag, double lat, double lon);
    void routeReady(int distance, int duration);
    void staticMapReady(const QByteArray &imageData, State forState);
    void placeSuggestionsReady(const QJsonArray &results);
    void errorOccurred(const QString &error);

private slots:
    void onReplyFinished(QNetworkReply *reply);
    void onThrottleTimer();

private:
    void doRequestStaticMap(const QString &centerCoord, bool immediate = false);
    void refreshMap(bool immediate = false);

    QTimer *m_throttleTimer;
    QString m_pendingCenter;
    static const int STATIC_MAP_INTERVAL_MS = 5000;

    QNetworkAccessManager *m_netManager;

    State m_state = STATE_LOCATING;

    // 百度BD09坐标
    double m_gpsLat       = 0;
    double m_gpsLon       = 0;
    bool m_gpsInitialized = false;

    double m_centerLat    = 0;
    double m_centerLon    = 0;
    QString m_markerPos;
    int m_zoom = 15;

    // 导航数据
    QString m_startLatLon;
    QString m_endLatLon;
    QString m_pathPoints;

    static const QString BAIDU_AK;
};

#endif // MAPMANAGER_H
