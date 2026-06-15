#ifndef NAVIGATION_PAGE_H
#define NAVIGATION_PAGE_H

#include <QWidget>
#include <QPixmap>
#include <QPoint>
#include <QListWidget>
#include <QJsonArray>
#include <QJsonObject>

class MapManager;
class EC20Manager;

QT_BEGIN_NAMESPACE
namespace Ui
{
    class NavigationPage;
}
QT_END_NAMESPACE

class NavigationPage : public QWidget
{
    Q_OBJECT

public:
    explicit NavigationPage(QWidget *parent = nullptr);
    ~NavigationPage() override;

    void zoomIn() { onZoomIn(); }
    void zoomOut() { onZoomOut(); }
    void zoomMax();
    void zoomMin();
    void resetView() { onReset(); }
    void startNavigate() { onNavigateClicked(); }
    void navigateTo(const QString &address); // 设置目的地并开始导航

public slots:
    void onPositionUpdated(double lon, double lat);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void onNavigateClicked();
    void onZoomIn();
    void onZoomOut();
    void onReset();
    void onClearOrigin();
    void onClearDestination();
    void onGpsTimerTimeout();

    // MapManager 回调
    void onCoordConverted(double bdLat, double bdLon);
    void onGeocodingDone(const QString &tag, double lat, double lon);
    void onPlaceSuggestionsReady(const QJsonArray &results);
    void onRouteReady(int distance, int duration);
    void onMapImageReady(const QByteArray &data, int forState);
    void onSuggestionTimer();
    void onSuggestionClicked(QListWidgetItem *item);

private:
    void setupGps();
    void setupConnections();
    double calculateDistance(double lat1, double lon1, double lat2, double lon2) const;
    bool parseCoordinate(const QString &text, double &lat, double &lon);

    Ui::NavigationPage *ui;
    MapManager *m_mapManager;

    // GPS
    double m_currentLat  = 0;
    double m_currentLon  = 0;
    bool m_hasCurrentPos = false;
    QTimer *m_gpsTimer;

    // 地图图像
    QPixmap m_mapImage;
    QPoint m_lastMousePos;
    bool m_isDragging = false;

    // 上次请求 GPS 的位置（用于判断是否需要更新地图）
    double m_lastRequestedLat = 0;
    double m_lastRequestedLon = 0;

    // 待用起点坐标（地理编码终点地址时使用）
    double m_pendingStartLat    = 0;
    double m_pendingStartLon    = 0;
    bool m_pendingStartIsBD09   = false;

    // 地点建议
    QListWidget *m_suggestionList        = nullptr;
    QTimer *m_suggestionTimer            = nullptr;
    QLineEdit *m_activeSuggestionInput   = nullptr;
    QString m_currentCity                = QStringLiteral("广州市");
    bool m_suppressSuggestion            = false;

    // 建议点击保存的坐标（BD09），导航时直接使用，跳过 geocoding
    double m_startSuggestionLat = 0;
    double m_startSuggestionLon = 0;
    double m_endSuggestionLat   = 0;
    double m_endSuggestionLon   = 0;
};

#endif // NAVIGATION_PAGE_H
