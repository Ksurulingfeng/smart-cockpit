#include "navigationPage.h"
#include "ui_navigationPage.h"
#include "mapmanager.h"
#include "ec20manager.h"
#include "configmanager.h"
#include "iconhelper.h"
#include <QPainter>
#include <QMouseEvent>
#include <QDebug>
#include <QtMath>
#include <QRegularExpression>
#include <QTimer>
#include <QScroller>

// 默认位置（无 GPS 信号时使用）
static const double DEFAULT_LAT = 23.105209683;
static const double DEFAULT_LON = 113.276236400;

NavigationPage::NavigationPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::NavigationPage)
    , m_mapManager(nullptr)
    , m_gpsTimer(nullptr)
{
    ui->setupUi(this);
    setupConnections();
    setupGps();
}

NavigationPage::~NavigationPage()
{
    delete ui;
}

void NavigationPage::setupConnections()
{
    // 地图管理器
    m_mapManager = new MapManager(this);
    connect(m_mapManager, &MapManager::coordConverted,
            this, &NavigationPage::onCoordConverted);
    connect(m_mapManager, &MapManager::geocodingDone,
            this, &NavigationPage::onGeocodingDone);
    connect(m_mapManager, &MapManager::placeSuggestionsReady,
            this, &NavigationPage::onPlaceSuggestionsReady);
    connect(m_mapManager, &MapManager::routeReady,
            this, &NavigationPage::onRouteReady);
    connect(m_mapManager, &MapManager::staticMapReady,
            this, &NavigationPage::onMapImageReady);
    connect(m_mapManager, &MapManager::errorOccurred,
            [](const QString &err) { qDebug() << "[Map]" << err; });

    // 导航按钮
    connect(ui->toolButton_navigate, &QToolButton::clicked,
            this, &NavigationPage::onNavigateClicked);

    // 清除按钮
    connect(ui->toolButton_origin_clear, &QToolButton::clicked,
            this, &NavigationPage::onClearOrigin);
    connect(ui->toolButton_destination_clear, &QToolButton::clicked,
            this, &NavigationPage::onClearDestination);

    // 缩放按钮
    connect(ui->pushButton_zoomIn, &QPushButton::clicked,
            this, &NavigationPage::onZoomIn);
    connect(ui->pushButton_zoomOut, &QPushButton::clicked,
            this, &NavigationPage::onZoomOut);
    connect(ui->pushButton_reset, &QPushButton::clicked,
            this, &NavigationPage::onReset);

    // 图标染色
    IconHelper::setupButton(ui->toolButton_origin_clear, Qt::black);
    IconHelper::setupButton(ui->toolButton_destination_clear, Qt::black);

    // 起点占位提示
    ui->lineEdit_origin->setPlaceholderText("留空则使用当前定位");

    // 回车触发导航（隐藏建议列表）
    connect(ui->lineEdit_destination, &QLineEdit::returnPressed, this, [this]() {
        m_suggestionList->hide();
        onNavigateClicked();
    });

    // 地点建议列表（手指滑动，无滚动条）
    m_suggestionList = new QListWidget(this);
    m_suggestionList->setMaximumHeight(180);
    m_suggestionList->hide();
    m_suggestionList->setFocusPolicy(Qt::NoFocus);
    m_suggestionList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_suggestionList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_suggestionList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    QScroller::grabGesture(m_suggestionList->viewport(), QScroller::LeftMouseButtonGesture);
    ui->verticalLayout->insertWidget(4, m_suggestionList);
    connect(m_suggestionList, &QListWidget::itemClicked,
            this, &NavigationPage::onSuggestionClicked);

    // 防抖定时器 + 两个输入框共享建议逻辑
    m_suggestionTimer = new QTimer(this);
    m_suggestionTimer->setSingleShot(true);
    connect(m_suggestionTimer, &QTimer::timeout,
            this, &NavigationPage::onSuggestionTimer);

    auto setupSuggestionInput = [this](QLineEdit *input) {
        connect(input, &QLineEdit::textChanged, this, [this, input]() {
            m_activeSuggestionInput = input;
            if (m_suppressSuggestion)
                return;
            if (input->text().trimmed().isEmpty()) {
                m_suggestionTimer->stop();
                m_suggestionList->hide();
                return;
            }
            m_suggestionTimer->start(300);
        });
    };
    setupSuggestionInput(ui->lineEdit_origin);
    setupSuggestionInput(ui->lineEdit_destination);
    m_activeSuggestionInput = ui->lineEdit_destination;

    // 地图拖拽（按下/松开，不做拖拽动画）
    ui->widget_map->setMouseTracking(false);
}

void NavigationPage::setupGps()
{
    // GPS 超时定时器
    m_gpsTimer = new QTimer(this);
    m_gpsTimer->setSingleShot(true);
    connect(m_gpsTimer, &QTimer::timeout,
            this, &NavigationPage::onGpsTimerTimeout);

    // 立即用默认位置加载地图（GPS 数据到达后会自动更新）
    onGpsTimerTimeout();
}

void NavigationPage::onPositionUpdated(double lon, double lat)
{
    // 注意：这里 EC20Manager 的 positionUpdated 信号参数是 (lon, lat)
    m_currentLat    = lat;
    m_currentLon    = lon;
    m_hasCurrentPos = true;

    m_mapManager->setGpsPosition(lat, lon);

    // 停止 GPS 超时定时器
    if (m_gpsTimer->isActive())
        m_gpsTimer->stop();

    bool isFirstLoc = (m_lastRequestedLat == 0 && m_lastRequestedLon == 0);
    // 经纬度差转米：1度≈111320米（纬度方向）
    double dLatM = (lat - m_lastRequestedLat) * 111320.0;
    double dLonM = (lon - m_lastRequestedLon) * 111320.0 * qCos(qDegreesToRadians(lat));
    double distM = qSqrt(dLatM * dLatM + dLonM * dLonM);

    // 首次定位或移动超过约10米才更新地图
    if (m_mapManager->state() == MapManager::STATE_LOCATING &&
        (isFirstLoc || distM > 10.0)) {
        m_lastRequestedLat = lat;
        m_lastRequestedLon = lon;
        m_mapManager->requestCoordConv(lat, lon);
    }

    // 自动填充起点（仅在起点为空时）
    if (ui->lineEdit_origin->text().isEmpty()) {
        ui->lineEdit_origin->setText(
            QString("%1, %2").arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6));
    }

    // 更新距离
    if (!ui->lineEdit_destination->text().isEmpty()) {
        double destLat = 0, destLon = 0;
        if (parseCoordinate(ui->lineEdit_destination->text(), destLat, destLon)) {
            double dist = calculateDistance(lat, lon, destLat, destLon);
            if (dist >= 1000)
                ui->label_distance->setText(QString("预计距离：%1 km").arg(dist / 1000.0, 0, 'f', 1));
            else
                ui->label_distance->setText(QString("预计距离：%1 m").arg((int)dist));
        }
    }
}

void NavigationPage::onGpsTimerTimeout()
{
    qDebug() << "[导航] GPS超时,使用默认位置(广州)";
    m_currentLat    = DEFAULT_LAT;
    m_currentLon    = DEFAULT_LON;
    m_hasCurrentPos = true;
    m_mapManager->setGpsPosition(DEFAULT_LAT, DEFAULT_LON);
    m_mapManager->requestCoordConv(DEFAULT_LAT, DEFAULT_LON);
}

void NavigationPage::onCoordConverted(double bdLat, double bdLon)
{
    qDebug() << "[导航] 坐标纠偏完成:" << bdLat << bdLon;
}

void NavigationPage::onGeocodingDone(const QString &tag, double lat, double lon)
{
    qDebug() << "[导航] 地理编码完成:" << tag << lat << lon;

    if (tag == "START") {
        // 起点地址解析完毕（lat/lon 已是 BD09），保存后继续解析终点
        m_pendingStartLat    = lat;
        m_pendingStartLon    = lon;
        m_pendingStartIsBD09 = true;

        QString endText = ui->lineEdit_destination->text().trimmed();
        double destLat, destLon;
        if (parseCoordinate(endText, destLat, destLon)) {
            // 终点是用户输入的 WGS84 坐标，转为 BD09
            double bdDestLat, bdDestLon;
            MapManager::wgs84ToBd09(destLat, destLon, bdDestLat, bdDestLon);
            m_mapManager->requestRoutePlan(
                QString("%1,%2").arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6),
                QString("%1,%2").arg(bdDestLat, 0, 'f', 6).arg(bdDestLon, 0, 'f', 6));
        } else {
            m_mapManager->requestGeocoding(endText, "END");
        }
    } else if (tag == "END") {
        // 终点地址解析完毕 → 路线规划
        double originLat = 0, originLon = 0;
        if (m_pendingStartLat != 0 || m_pendingStartLon != 0) {
            if (m_pendingStartIsBD09) {
                originLat = m_pendingStartLat;
                originLon = m_pendingStartLon;
                qDebug() << "[导航] 起点 BD09(来自地理编码):" << originLat << originLon;
            } else {
                qDebug() << "[导航] 起点 WGS84:" << m_pendingStartLat << m_pendingStartLon;
                MapManager::wgs84ToBd09(m_pendingStartLat, m_pendingStartLon, originLat, originLon);
                qDebug() << "[导航] 起点 BD09(转换后):" << originLat << originLon;
            }
            m_pendingStartLat    = 0;
            m_pendingStartLon    = 0;
            m_pendingStartIsBD09 = false;
        } else {
            QStringList parts = m_mapManager->startLatLon().split(',');
            if (parts.size() == 2) {
                originLon = parts[0].toDouble();
                originLat = parts[1].toDouble();
            }
        }
        qDebug() << "[导航] 终点 BD09:" << lat << lon;
        m_mapManager->requestRoutePlan(
            QString("%1,%2").arg(originLat, 0, 'f', 6).arg(originLon, 0, 'f', 6),
            QString("%1,%2").arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6));
    }
}

void NavigationPage::onRouteReady(int distance, int duration)
{
    if (distance >= 1000)
        ui->label_distance->setText(
            QString("预计距离：%1 km").arg(distance / 1000.0, 0, 'f', 1));
    else
        ui->label_distance->setText(QString("预计距离：%1 m").arg(distance));
    ui->label_time->setText(QString("预计用时：%1 分钟").arg(duration / 60));
}

void NavigationPage::onMapImageReady(const QByteArray &data, int forState)
{
    qDebug() << "[导航] 地图图片到达:" << data.size() << "字节, 模式:" << (forState == 0 ? "定位" : "导航");
    QPixmap pix;
    if (pix.loadFromData(data)) {
        m_mapImage = pix;
        qDebug() << "[导航] 地图渲染成功:" << pix.size();
        update();
    } else {
        qDebug() << "[导航] 地图图片解析失败";
    }
}

// ==================== 绘制 ====================

void NavigationPage::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    QRect mapRect = ui->widget_map->geometry();

    if (!m_mapImage.isNull()) {
        QPixmap scaled = m_mapImage.scaled(mapRect.size(), Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation);
        painter.drawPixmap(mapRect.topLeft(), scaled);
    } else {
        painter.fillRect(mapRect, QColor(30, 30, 35));
        painter.setPen(Qt::white);
        painter.drawText(mapRect, Qt::AlignCenter, "正在加载地图...\n(需要百度AK和网络连接)");
    }
}

// ==================== 用户操作 ====================

void NavigationPage::onNavigateClicked()
{
    QString startText = ui->lineEdit_origin->text().trimmed();
    QString endText   = ui->lineEdit_destination->text().trimmed();

    if (endText.isEmpty()) {
        qDebug() << "[导航] 请输入终点";
        return;
    }

    m_suggestionList->hide();
    m_mapManager->setState(MapManager::STATE_NAVIGATING);

    // --- 解析起点 ---
    double startLat = 0, startLon = 0;
    bool startIsBD09 = false;
    bool startResolved = false;
    bool startIsCoord = parseCoordinate(startText, startLat, startLon);
    if (!startIsCoord && !startText.isEmpty()) {
        // 起点是地址 → 先检查是否有建议坐标可用（BD09）
        if (m_startSuggestionLat != 0 || m_startSuggestionLon != 0) {
            startLat     = m_startSuggestionLat;
            startLon     = m_startSuggestionLon;
            startIsBD09  = true;
            startResolved = true;
            m_startSuggestionLat = 0;
            m_startSuggestionLon = 0;
        } else {
            m_mapManager->requestGeocoding(startText, "START");
            return;
        }
    }
    if (!startIsCoord && !startResolved) {
        // 起点为空 → 使用当前位置（WGS84）
        if (!m_hasCurrentPos) {
            startLat = DEFAULT_LAT;
            startLon = DEFAULT_LON;
        } else {
            startLat = m_currentLat;
            startLon = m_currentLon;
        }
    }

    // --- 解析终点 ---
    double destLat = 0, destLon = 0;
    bool destIsBD09 = false;
    bool destIsCoord = parseCoordinate(endText, destLat, destLon);
    if (!destIsCoord) {
        // 终点是地址 → 先检查是否有建议坐标可用（BD09）
        if (m_endSuggestionLat != 0 || m_endSuggestionLon != 0) {
            destLat    = m_endSuggestionLat;
            destLon    = m_endSuggestionLon;
            destIsBD09 = true;
            m_endSuggestionLat = 0;
            m_endSuggestionLon = 0;
        } else {
            m_pendingStartLat    = startLat;
            m_pendingStartLon    = startLon;
            m_pendingStartIsBD09 = startIsBD09;
            m_mapManager->requestGeocoding(endText, "END");
            return;
        }
    }

    // --- 统一坐标系为 BD09 ---
    double bdStartLat, bdStartLon;
    if (startIsBD09) {
        bdStartLat = startLat;
        bdStartLon = startLon;
    } else {
        MapManager::wgs84ToBd09(startLat, startLon, bdStartLat, bdStartLon);
    }

    double bdDestLat, bdDestLon;
    if (destIsBD09) {
        bdDestLat = destLat;
        bdDestLon = destLon;
    } else {
        MapManager::wgs84ToBd09(destLat, destLon, bdDestLat, bdDestLon);
    }

    qDebug() << "[导航] 路线规划 BD09:" << bdStartLat << bdStartLon << "→" << bdDestLat << bdDestLon;
    m_mapManager->requestRoutePlan(
        QString("%1,%2").arg(bdStartLat, 0, 'f', 6).arg(bdStartLon, 0, 'f', 6),
        QString("%1,%2").arg(bdDestLat, 0, 'f', 6).arg(bdDestLon, 0, 'f', 6));
}

void NavigationPage::onZoomIn()
{
    qDebug() << "[导航] 放大";
    m_mapManager->zoomIn();
}
void NavigationPage::onZoomOut()
{
    qDebug() << "[导航] 缩小";
    m_mapManager->zoomOut();
}
void NavigationPage::zoomMax()
{
    qDebug() << "[导航] 放到最大";
    m_mapManager->setZoom(19);
}
void NavigationPage::zoomMin()
{
    qDebug() << "[导航] 放到最小";
    m_mapManager->setZoom(3);
}
void NavigationPage::onReset()
{
    qDebug() << "[导航] 重置";
    m_mapManager->resetView();
}

void NavigationPage::onClearOrigin()
{
    ui->lineEdit_origin->clear();
    ui->label_distance->setText("预计距离：");
    ui->label_time->setText("预计用时：");
}

void NavigationPage::onClearDestination()
{
    ui->lineEdit_destination->clear();
    m_suggestionList->hide();
    ui->label_distance->setText("预计距离：");
    ui->label_time->setText("预计用时：");
}

// ==================== 鼠标拖拽 ====================

void NavigationPage::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && ui->widget_map->geometry().contains(event->pos())) {
        m_lastMousePos = event->pos();
        m_isDragging   = true;
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void NavigationPage::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_isDragging) {
        m_isDragging = false;
        int dx       = event->pos().x() - m_lastMousePos.x();
        int dy       = event->pos().y() - m_lastMousePos.y();
        if (dx != 0 || dy != 0) {
            qDebug() << "[导航] 拖拽偏移:" << dx << dy;
            QRect r = ui->widget_map->geometry();
            m_mapManager->panByPixels(dx, dy, r.width(), r.height());
        }
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void NavigationPage::navigateTo(const QString &address)
{
    qDebug() << "[导航] 导航到:" << address;
    ui->lineEdit_destination->setText(address);
    onNavigateClicked();
}

// ==================== 地点建议 ====================

void NavigationPage::onSuggestionTimer()
{
    if (!m_activeSuggestionInput)
        return;
    const QString text = m_activeSuggestionInput->text().trimmed();
    if (text.isEmpty())
        return;
    m_mapManager->requestPlaceSuggestion(text, m_currentCity);
}

void NavigationPage::onPlaceSuggestionsReady(const QJsonArray &results)
{
    m_suggestionList->clear();

    if (results.isEmpty()) {
        m_suggestionList->addItem("未找到匹配地点");
        m_suggestionList->show();
        return;
    }

    for (const QJsonValue &v : results) {
        QJsonObject obj   = v.toObject();
        QString name      = obj["name"].toString();
        QString address   = obj["address"].toString();
        double lat        = obj["location"].toObject()["lat"].toDouble();
        double lng        = obj["location"].toObject()["lng"].toDouble();

        QString label = QString("%1 — %2").arg(name, address);
        auto *item    = new QListWidgetItem(label, m_suggestionList);
        item->setData(Qt::UserRole, name);
        item->setData(Qt::UserRole + 1, QString("%1,%2").arg(lat, 0, 'f', 6).arg(lng, 0, 'f', 6));
    }

    m_suggestionList->show();
}

void NavigationPage::onSuggestionClicked(QListWidgetItem *item)
{
    QString name = item->data(Qt::UserRole).toString();
    QString coords = item->data(Qt::UserRole + 1).toString();
    if (name.isEmpty() || !m_activeSuggestionInput)
        return;

    // 保存建议坐标（BD09），导航时跳过 geocoding
    QStringList parts = coords.split(',');
    if (parts.size() == 2) {
        double lat = parts[0].toDouble();
        double lng = parts[1].toDouble();
        if (m_activeSuggestionInput == ui->lineEdit_origin) {
            m_startSuggestionLat = lat;
            m_startSuggestionLon = lng;
        } else {
            m_endSuggestionLat = lat;
            m_endSuggestionLon = lng;
        }
    }

    m_suppressSuggestion = true;
    m_activeSuggestionInput->setText(name);
    m_suppressSuggestion = false;
    m_suggestionList->hide();
}

// ==================== 工具函数 ====================

double NavigationPage::calculateDistance(double lat1, double lon1,
                                         double lat2, double lon2) const
{
    double R    = 6371000.0;
    double dLat = qDegreesToRadians(lat2 - lat1);
    double dLon = qDegreesToRadians(lon2 - lon1);
    double a    = qSin(dLat / 2) * qSin(dLat / 2) +
               qCos(qDegreesToRadians(lat1)) * qCos(qDegreesToRadians(lat2)) *
                   qSin(dLon / 2) * qSin(dLon / 2);
    return R * 2 * qAtan2(qSqrt(a), qSqrt(1 - a));
}

bool NavigationPage::parseCoordinate(const QString &text, double &lat, double &lon)
{
    static QRegularExpression re(R"(([+-]?\d+\.?\d*)\s*[,\s]\s*([+-]?\d+\.?\d*))");
    QRegularExpressionMatch match = re.match(text.trimmed());
    if (match.hasMatch()) {
        lat = match.captured(1).toDouble();
        lon = match.captured(2).toDouble();
        if (lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180)
            return true;
    }
    return false;
}
