#include "mapmanager.h"
#include "config.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QDebug>
#include <QtMath>

const QString MapManager::BAIDU_AK = Config::BAIDU_AK;

MapManager::MapManager(QObject *parent)
    : QObject(parent)
{
    m_netManager = new QNetworkAccessManager(this);
    connect(m_netManager, &QNetworkAccessManager::finished,
            this, &MapManager::onReplyFinished);

    m_throttleTimer = new QTimer(this);
    m_throttleTimer->setSingleShot(true);
    connect(m_throttleTimer, &QTimer::timeout,
            this, &MapManager::onThrottleTimer);
}

MapManager::~MapManager() = default;

void MapManager::setState(State s)
{
    m_state = s;
}

void MapManager::setCenter(double lat, double lon)
{
    m_centerLat = lat;
    m_centerLon = lon;
}

void MapManager::setZoom(int z)
{
    m_zoom = qBound(3, z, 19);
    refreshMap();
}

void MapManager::zoomIn()
{
    if (m_zoom < 19) {
        m_zoom++;
        refreshMap(true);
    }
}

void MapManager::zoomOut()
{
    if (m_zoom > 3) {
        m_zoom--;
        refreshMap(true);
    }
}

void MapManager::resetView()
{
    if (m_state == STATE_NAVIGATING && !m_startLatLon.isEmpty()) {
        QStringList parts = m_startLatLon.split(',');
        if (parts.size() == 2) {
            m_centerLat = parts[1].toDouble();
            m_centerLon = parts[0].toDouble();
        }
    } else {
        // 定位模式：通过 requestCoordConv 做 WGS84→BD09 转换，避免坐标偏移
        m_zoom = 14;
        if (m_gpsLat != 0 && m_gpsLon != 0) {
            requestCoordConv(m_gpsLat, m_gpsLon, true);
            return;
        }
    }
    m_zoom = 14;
    refreshMap(true);
}

void MapManager::panByPixels(int dx, int dy, int viewWidth, int viewHeight)
{
    Q_UNUSED(viewHeight);
    double lonPerPixel = 360.0 / (256.0 * qPow(2, m_zoom)) * (viewWidth / 256.0);
    double latPerPixel = 180.0 / (256.0 * qPow(2, m_zoom)) * (viewWidth / 256.0);
    m_centerLon -= dx * lonPerPixel;
    m_centerLat += dy * latPerPixel;
    m_centerLat = qBound(-85.0, m_centerLat, 85.0);
    if (m_centerLon > 180) m_centerLon -= 360;
    if (m_centerLon < -180) m_centerLon += 360;
    refreshMap(true);
}

void MapManager::setGpsPosition(double lat, double lon)
{
    m_gpsLat         = lat;
    m_gpsLon         = lon;
    m_gpsInitialized = true;
}

// ==================== WGS84→BD09 本地转换 ====================

static const double PI   = 3.14159265358979323846;
static const double X_PI = PI * 3000.0 / 180.0;
static const double A    = 6378245.0;
static const double EE   = 0.00669342162296594323;

static bool outOfChina(double lat, double lon)
{
    return (lon < 72.004 || lon > 137.8347 || lat < 0.8293 || lat > 55.8271);
}

static double transformLat(double x, double y)
{
    double ret = -100.0 + 2.0 * x + 3.0 * y + 0.2 * y * y + 0.1 * x * y + 0.2 * sqrt(fabs(x));
    ret += (20.0 * sin(6.0 * x * PI) + 20.0 * sin(2.0 * x * PI)) * 2.0 / 3.0;
    ret += (20.0 * sin(y * PI) + 40.0 * sin(y / 3.0 * PI)) * 2.0 / 3.0;
    ret += (160.0 * sin(y / 12.0 * PI) + 320.0 * sin(y * PI / 30.0)) * 2.0 / 3.0;
    return ret;
}

static double transformLon(double x, double y)
{
    double ret = 300.0 + x + 2.0 * y + 0.1 * x * x + 0.1 * x * y + 0.1 * sqrt(fabs(x));
    ret += (20.0 * sin(6.0 * x * PI) + 20.0 * sin(2.0 * x * PI)) * 2.0 / 3.0;
    ret += (20.0 * sin(x * PI) + 40.0 * sin(x / 3.0 * PI)) * 2.0 / 3.0;
    ret += (150.0 * sin(x / 12.0 * PI) + 300.0 * sin(x / 30.0 * PI)) * 2.0 / 3.0;
    return ret;
}

static void wgs84ToGcj02(double wgsLat, double wgsLon, double &gcjLat, double &gcjLon)
{
    if (outOfChina(wgsLat, wgsLon)) {
        gcjLat = wgsLat;
        gcjLon = wgsLon;
        return;
    }
    double dLat      = transformLat(wgsLon - 105.0, wgsLat - 35.0);
    double dLon      = transformLon(wgsLon - 105.0, wgsLat - 35.0);
    double radLat    = wgsLat / 180.0 * PI;
    double magic     = sin(radLat);
    magic            = 1.0 - EE * magic * magic;
    double sqrtMagic = sqrt(magic);
    dLat             = (dLat * 180.0) / ((A * (1.0 - EE)) / (magic * sqrtMagic) * PI);
    dLon             = (dLon * 180.0) / (A / sqrtMagic * cos(radLat) * PI);
    gcjLat           = wgsLat + dLat;
    gcjLon           = wgsLon + dLon;
}

static void gcj02ToBd09(double gcjLat, double gcjLon, double &bdLat, double &bdLon)
{
    double x = gcjLon, y = gcjLat;
    double z     = sqrt(x * x + y * y) + 0.00002 * sin(y * X_PI);
    double theta = atan2(y, x) + 0.000003 * cos(x * X_PI);
    bdLon        = z * cos(theta) + 0.0065;
    bdLat        = z * sin(theta) + 0.006;
}

void MapManager::wgs84ToBd09(double wgsLat, double wgsLon, double &bdLat, double &bdLon)
{
    double gcjLat, gcjLon;
    wgs84ToGcj02(wgsLat, wgsLon, gcjLat, gcjLon);
    gcj02ToBd09(gcjLat, gcjLon, bdLat, bdLon);
}

// ==================== API 请求 ====================

void MapManager::requestCoordConv(double lat, double lon, bool immediate)
{
    double bdLat, bdLon;
    wgs84ToBd09(lat, lon, bdLat, bdLon);
    qDebug() << "[Map] WGS84→BD09:" << lat << lon << "→" << bdLat << bdLon;
    emit coordConverted(bdLat, bdLon);
    requestStaticMapForCoords(bdLat, bdLon, immediate);
}

void MapManager::requestPlaceSuggestion(const QString &keyword, const QString &region)
{
    if (keyword.trimmed().isEmpty())
        return;

    qDebug() << "[Map] 地点建议:" << keyword << "区域:" << region;
    QUrl url("http://api.map.baidu.com/place/v2/suggestion");
    QUrlQuery query;
    query.addQueryItem("query", keyword);
    query.addQueryItem("region", region.isEmpty() ? "全国" : region);
    query.addQueryItem("output", "json");
    query.addQueryItem("ak", BAIDU_AK);
    url.setQuery(query);
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::User, QStringLiteral("PLACE_SUGGESTION"));
    m_netManager->get(req);
}

void MapManager::requestGeocoding(const QString &address, const QString &tag)
{
    qDebug() << "[Map] 地理编码:" << tag << address;
    QUrl url("http://api.map.baidu.com/geocoding/v3/");
    QUrlQuery query;
    query.addQueryItem("address", address);
    query.addQueryItem("output", "json");
    query.addQueryItem("ak", BAIDU_AK);
    url.setQuery(query);
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::User, tag);
    m_netManager->get(req);
}

void MapManager::requestRoutePlan(const QString &originLatLon, const QString &destLatLon)
{
    // originLatLon 是 "lat,lon" 格式（来自 NavigationPage），转为 "lon,lat" 统一存储
    QStringList originParts = originLatLon.split(',');
    if (originParts.size() == 2)
        m_startLatLon = QString("%1,%2").arg(originParts[1].trimmed(), originParts[0].trimmed());
    QStringList destParts = destLatLon.split(',');
    if (destParts.size() == 2)
        m_endLatLon = QString("%1,%2").arg(destParts[1].trimmed(), destParts[0].trimmed());

    QUrl url("http://api.map.baidu.com/directionlite/v1/driving");
    QUrlQuery query;
    query.addQueryItem("origin", originLatLon);
    query.addQueryItem("destination", destLatLon);
    query.addQueryItem("coord_type", "bd09ll");
    query.addQueryItem("ak", BAIDU_AK);
    query.addQueryItem("output", "json");
    url.setQuery(query);
    qDebug() << "[Map] 路线规划请求:" << url.toString();
    m_netManager->get(QNetworkRequest(url));
}

void MapManager::requestStaticMapForCoords(double bdLat, double bdLon, bool immediate)
{
    qDebug() << "[Map] 静态图请求 定位模式 中心:" << bdLat << bdLon
             << "缩放:" << m_zoom << (immediate ? "立即" : "冷却中");
    m_state        = STATE_LOCATING;
    m_centerLat    = bdLat;
    m_centerLon    = bdLon;
    m_markerPos    = QString("%1,%2").arg(bdLon, 0, 'f', 6).arg(bdLat, 0, 'f', 6);
    QString center = QString("%1,%2").arg(m_centerLon, 0, 'f', 6).arg(m_centerLat, 0, 'f', 6);
    doRequestStaticMap(center, immediate);
}

void MapManager::requestStaticMapWithPath(bool immediate)
{
    m_state        = STATE_NAVIGATING;
    QString center = QString("%1,%2").arg(m_centerLon, 0, 'f', 6).arg(m_centerLat, 0, 'f', 6);
    doRequestStaticMap(center, immediate);
}

void MapManager::doRequestStaticMap(const QString &centerCoord, bool immediate)
{
    // 非立即模式且冷却中：暂存最新中心点，定时器触发后再发
    if (!immediate && m_throttleTimer->isActive()) {
        m_pendingCenter = centerCoord;
        return;
    }

    QUrl url("http://api.map.baidu.com/staticimage/v2");
    QUrlQuery query;
    query.addQueryItem("ak", BAIDU_AK);
    query.addQueryItem("width", "712");
    query.addQueryItem("height", "480");
    query.addQueryItem("zoom", QString::number(m_zoom));
    query.addQueryItem("center", centerCoord);

    if (m_state == STATE_NAVIGATING && !m_pathPoints.isEmpty()) {
        query.addQueryItem("pathStyles", "0x00ff00,3,1");
        query.addQueryItem("paths", m_pathPoints);
    } else if (m_state == STATE_LOCATING && !m_markerPos.isEmpty()) {
        query.addQueryItem("markers", m_markerPos);
        query.addQueryItem("markerStyles", "l,S,0xff0000");
    }

    url.setQuery(query);
    m_netManager->get(QNetworkRequest(url));
    m_throttleTimer->start(STATIC_MAP_INTERVAL_MS);
}

void MapManager::onThrottleTimer()
{
    // 冷却结束，用最新暂存的位置发一次请求
    if (!m_pendingCenter.isEmpty()) {
        QString center = m_pendingCenter;
        m_pendingCenter.clear();
        doRequestStaticMap(center);
    }
}

void MapManager::refreshMap(bool immediate)
{
    if (m_state == STATE_NAVIGATING && !m_pathPoints.isEmpty())
        requestStaticMapWithPath(immediate);
    else if (m_centerLat != 0 && m_centerLon != 0) {
        m_markerPos = QString("%1,%2").arg(m_centerLon, 0, 'f', 6).arg(m_centerLat, 0, 'f', 6);
        requestStaticMapForCoords(m_centerLat, m_centerLon, immediate);
    } else if (m_gpsLat != 0 && m_gpsLon != 0)
        requestCoordConv(m_gpsLat, m_gpsLon);
}

// ==================== 统一回调 ====================

void MapManager::onReplyFinished(QNetworkReply *reply)
{
    QByteArray data = reply->readAll();
    QString urlStr  = reply->url().toString();
    int httpCode    = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError && httpCode != 200) {
        qDebug() << "[Map] 网络错误:" << reply->errorString();
        emit errorOccurred(reply->errorString());
        reply->deleteLater();
        return;
    }

    // 1. 坐标转换
    if (urlStr.contains("geoconv/v2")) {
        QJsonObject root = QJsonDocument::fromJson(data).object();
        if (root["status"].toInt() == 0) {
            QJsonArray arr = root["result"].toArray();
            if (!arr.isEmpty()) {
                QJsonObject r = arr[0].toObject();
                double bdLat  = r["y"].toDouble();
                double bdLon  = r["x"].toDouble();
                emit coordConverted(bdLat, bdLon);
                requestStaticMapForCoords(bdLat, bdLon);
            }
        }
    }
    // 2. 地理编码
    else if (urlStr.contains("geocoding/v3")) {
        QString tag      = reply->request().attribute(QNetworkRequest::User).toString();
        QJsonObject root = QJsonDocument::fromJson(data).object();
        if (root["status"].toInt() == 0) {
            QJsonObject loc = root["result"].toObject()["location"].toObject();
            double lat      = loc["lat"].toDouble();
            double lon      = loc["lng"].toDouble();
            emit geocodingDone(tag, lat, lon);
            // 路线规划由 NavigationPage::onGeocodingDone 统一处理（含 WGS84→BD09 转换）
        } else {
            emit errorOccurred("地理编码失败: " + QString::number(root["status"].toInt()));
        }
    }
    // 2.5. 地点建议
    else if (urlStr.contains("place/v2/suggestion")) {
        QJsonObject root = QJsonDocument::fromJson(data).object();
        if (root["status"].toInt() == 0) {
            QJsonArray results = root["result"].toArray();
            emit placeSuggestionsReady(results);
        } else {
            qDebug() << "[Map] 地点建议失败, status:" << root["status"].toInt();
        }
    }
    // 3. 路径规划
    else if (urlStr.contains("directionlite")) {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject root  = doc.object();
        if (root["status"].toInt() == 0) {
            QJsonArray routes = root["result"].toObject()["routes"].toArray();
            if (routes.isEmpty()) {
                emit errorOccurred("未找到路线");
                reply->deleteLater();
                return;
            }
            QJsonObject route = routes[0].toObject();
            int distance      = route["distance"].toInt();
            int duration      = route["duration"].toInt();
            emit routeReady(distance, duration);

            // 提取路径点
            QStringList allPoints;
            QJsonArray steps = route["steps"].toArray();
            for (const QJsonValue &v : steps) {
                QString pathStr = v.toObject()["path"].toString();
                allPoints.append(pathStr.split(';'));
            }
            // 采样减少点数
            QStringList sampled;
            for (int i = 0; i < allPoints.size(); i += 5)
                sampled.append(allPoints[i]);
            m_pathPoints = sampled.join(';');

            // 地图中心设为路径起点
            if (!sampled.isEmpty()) {
                QStringList coords = sampled.first().split(',');
                if (coords.size() == 2) {
                    m_centerLon = coords[0].toDouble();
                    m_centerLat = coords[1].toDouble();
                }
            }
            requestStaticMapWithPath();
        } else {
            QString errMsg = QString::fromUtf8(data);
            qDebug() << "[Map] 路线规划失败, status:" << root["status"].toInt() << "message:" << root["message"].toString() << "body:" << errMsg.left(200);
            emit errorOccurred("路线规划失败, status=" + QString::number(root["status"].toInt()));
        }
    }
    // 4. 静态地图
    else if (urlStr.contains("staticimage/v2")) {
        // 检查 PNG 魔数 \x89PNG\r\n\x1A\n
        bool isPng = (data.size() >= 8 && (unsigned char)data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G');
        if (isPng) {
            emit staticMapReady(data, m_state);
        } else {
            QString errMsg = QString::fromUtf8(data);
            qDebug() << "[Map] 静态图失败, URL:" << urlStr
                     << "大小:" << data.size() << "响应:" << errMsg;
            emit errorOccurred("静态地图数据无效: " + errMsg);
        }
    }

    reply->deleteLater();
}
