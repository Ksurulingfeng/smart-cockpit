#include "rearViewCamera.h"
#include "v4l2camera.h"
#include "comlayer.h"
#include "config.h"
#include <QPainter>
#include <QDebug>
#include <QVBoxLayout>

RearViewCamera::RearViewCamera(QWidget *parent)
    : QWidget(parent)
    , m_camera(nullptr)
    , m_distance(-1)
    , m_active(false)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setStyleSheet("background-color: black;");

    m_alertTimer = new QTimer(this);
    m_alertTimer->setSingleShot(true);
    m_alertTimer->setInterval(5000);
    connect(m_alertTimer, &QTimer::timeout, this, &RearViewCamera::onAlertTimeout);
}

RearViewCamera::~RearViewCamera()
{
    stop();
    if (m_camera) {
        delete m_camera;
        m_camera = nullptr;
    }
}

void RearViewCamera::onGearChanged(int gear)
{
    if (gear == CAN_GEAR_R) {
        start(Config::RVC_DEVICE, Config::RVC_WIDTH, Config::RVC_HEIGHT);
    } else {
        stop();
    }
}

void RearViewCamera::onDistanceChanged(int cm)
{
    setDistance(cm);
}

void RearViewCamera::start(const QString &device, int width, int height)
{
    if (m_active)
        return;

    // 先销毁旧的摄像头对象（如果存在）
    if (m_camera) {
        m_camera->stop();
        m_camera->close();
        delete m_camera;
        m_camera = nullptr;
    }

    // 创建新的摄像头对象
    m_camera = new V4l2Camera(this);
    connect(m_camera, &V4l2Camera::imageReady, this, &RearViewCamera::onImageReady);

    if (m_camera->open(device.toUtf8().constData(), width, height)) {
        if (m_camera->start()) {
            m_active = true;
            showFullScreen();
            qDebug() << "倒车影像已启动，设备:" << device;
        } else {
            qDebug() << "摄像头启动失败";
            delete m_camera;
            m_camera = nullptr;
        }
    } else {
        qDebug() << "摄像头打开失败:" << device;
        delete m_camera;
        m_camera = nullptr;
    }
}

void RearViewCamera::stop()
{
    if (!m_active)
        return;

    if (m_camera) {
        m_camera->stop();
        m_camera->close();
        delete m_camera;
        m_camera = nullptr;
    }
    m_active       = false;
    m_currentFrame = QImage();
    m_alertSuppressed = false;
    if (m_alertActive) {
        m_alertActive = false;
        m_alertTimer->stop();
        ComLayer::instance()->sendSignal(SID_A_LED_STATE, CAN_LED_OFF);
        ComLayer::instance()->sendSignal(SID_A_BUZZER_MODE, CAN_BUZZER_OFF);
    }
    hide();
    qDebug() << "倒车影像已关闭";
}

void RearViewCamera::setDistance(int cm)
{
    bool wasBelow = (m_distance >= 0 && m_distance < Config::RVC_WARN_DIST_CM);
    m_distance    = cm;
    bool isBelow  = (cm >= 0 && cm < Config::RVC_WARN_DIST_CM);

    // 距离恢复后重置抑制标记，允许再次报警
    if (!isBelow)
        m_alertSuppressed = false;

    if (m_active) {
        if (isBelow && !wasBelow && !m_alertSuppressed) {
            m_alertActive = true;
            m_alertTimer->start();
            ComLayer::instance()->sendSignal(SID_A_LED_STATE, CAN_LED_ALERT);
            ComLayer::instance()->sendSignal(SID_A_BUZZER_MODE, CAN_BUZZER_ALERT);
        } else if (!isBelow && wasBelow) {
            m_alertActive = false;
            m_alertTimer->stop();
            ComLayer::instance()->sendSignal(SID_A_LED_STATE, CAN_LED_OFF);
            ComLayer::instance()->sendSignal(SID_A_BUZZER_MODE, CAN_BUZZER_OFF);
        }
    }

    update();
}

void RearViewCamera::onAlertTimeout()
{
    if (!m_alertActive)
        return;
    m_alertActive     = false;
    m_alertSuppressed = true; // 超时后抑制，距离恢复前不再触发
    ComLayer::instance()->sendSignal(SID_A_LED_STATE, CAN_LED_OFF);
    ComLayer::instance()->sendSignal(SID_A_BUZZER_MODE, CAN_BUZZER_OFF);
}

void RearViewCamera::onImageReady(const QImage &image)
{
    if (!m_active)
        return;
    m_currentFrame = image;
    update(); // 刷新显示视频
}

void RearViewCamera::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    if (!m_active)
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 1. 绘制视频画面
    if (!m_currentFrame.isNull()) {
        QImage scaled = scaleToFit(m_currentFrame);
        int x         = (width() - scaled.width()) / 2;
        int y         = (height() - scaled.height()) / 2;
        painter.drawImage(x, y, scaled);
    } else {
        painter.fillRect(rect(), Qt::black);
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, "等待摄像头数据...");
    }

    // 2. 顶部标题 "倒车影像"
    {
        QFont titleFont;
        titleFont.setPointSize(42);
        titleFont.setBold(true);
        painter.setFont(titleFont);

        QString title = "倒车影像";
        QFontMetrics fm(titleFont);
        QRect titleRect(0, 30, width(), fm.height() + 20);
        painter.fillRect(titleRect, QColor(0, 0, 0, 120));
        painter.setPen(QColor(255, 255, 255, 220));
        painter.drawText(titleRect, Qt::AlignCenter, title);
    }

    // 3. 距离显示 + 预警
    if (m_distance >= 0) {
        bool warn = (m_distance < Config::RVC_WARN_DIST_CM);
        QColor bg = warn ? QColor(255, 0, 0, 180) : QColor(0, 0, 0, 180);
        QColor fg = warn ? Qt::white : Qt::white;

        QString text = warn ? QString("⚠ 距离: %1 cm 立即停车！").arg(m_distance)
                            : QString("距离: %1 cm").arg(m_distance);

        QFont font = painter.font();
        font.setPointSize(warn ? 32 : 28);
        font.setBold(true);
        painter.setFont(font);

        QFontMetrics fm(font);
        int tw = fm.horizontalAdvance(text) + 40;
        QRect textRect(width() - tw - 20, height() - 90, tw, 70);
        painter.fillRect(textRect, bg);
        painter.setPen(fg);
        painter.drawText(textRect, Qt::AlignCenter, text);

        // 距离 < 10 时全屏红色闪烁边框
        if (warn) {
            QPen warnPen(Qt::red, 6);
            painter.setPen(warnPen);
            painter.drawRect(rect().adjusted(3, 3, -3, -3));
        }
    }

    // 4. 倒车辅助线
    QPen pen(Qt::green, 3);
    painter.setPen(pen);
    int cx     = width() / 2;
    int bottom = height() - 50;
    painter.drawLine(cx - 80, bottom, cx + 80, bottom); // 横线
    painter.drawLine(cx, bottom - 40, cx, bottom);      // 竖线
}

QImage RearViewCamera::scaleToFit(const QImage &image)
{
    QSize target = image.size();
    target.scale(size(), Qt::KeepAspectRatio);
    if (target == image.size())
        return image;
    return image.scaled(target, Qt::KeepAspectRatio, Qt::FastTransformation);
}