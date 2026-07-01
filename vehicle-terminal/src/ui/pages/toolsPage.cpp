#include "toolsPage.h"
#include "ui_toolsPage.h"
#include "config.h"
#include "configmanager.h"
#include "rte.h"
#include "com_stack/com.h"
#include <QDebug>

ToolsPage::ToolsPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ToolsPage)
{
    ui->setupUi(this);
    setupConnections();
    loadState();
}

ToolsPage::~ToolsPage()
{
    saveState();
    delete ui;
}

void ToolsPage::setupConnections()
{
    // 工具子页面
    connect(ui->toolButton_serialTest, &QToolButton::clicked,
            this, &ToolsPage::onSerialTestClicked);
    connect(ui->toolButton_canbusTest, &QToolButton::clicked,
            this, &ToolsPage::onCanbusTestClicked);
    connect(ui->toolButton_gpioTest, &QToolButton::clicked,
            this, &ToolsPage::onGpioTestClicked);

    // ===== 空调 =====
    connect(ui->switch_ac, &SwitchButton::toggled,
            this, &ToolsPage::onAcToggled);
    connect(ui->btn_fanUp, &QPushButton::clicked,
            this, &ToolsPage::onFanUp);
    connect(ui->btn_fanDown, &QPushButton::clicked,
            this, &ToolsPage::onFanDown);
    connect(ui->slider_fanSpeed, &QSlider::valueChanged,
            this, &ToolsPage::onFanSliderChanged);
    connect(ui->slider_fanSpeed, &QSlider::sliderReleased,
            this, [this]() { saveState(); });

    // ===== 车窗（长按连续升降） =====
    m_winRepeatTimer = new QTimer(this);
    m_winRepeatTimer->setInterval(100);
    connect(m_winRepeatTimer, &QTimer::timeout, this, &ToolsPage::onWinRepeat);

    connect(ui->btn_winUp, &QPushButton::pressed, this, [this]() { onWinPressed(1); });
    connect(ui->btn_winUp, &QPushButton::released, this, &ToolsPage::onWinReleased);
    connect(ui->btn_winDown, &QPushButton::pressed, this, [this]() { onWinPressed(-1); });
    connect(ui->btn_winDown, &QPushButton::released, this, &ToolsPage::onWinReleased);
    connect(ui->slider_window, &QSlider::valueChanged,
            this, &ToolsPage::onWinSliderChanged);
    connect(ui->slider_window, &QSlider::sliderReleased,
            this, [this]() { saveState(); });

    // 一键开/关窗
    connect(ui->btn_winFullOpen, &QPushButton::clicked, this, &ToolsPage::onWinFullOpen);
    connect(ui->btn_winFullClose, &QPushButton::clicked, this, &ToolsPage::onWinFullClose);

    // ===== 照明灯 =====
    connect(ui->switch_led, &SwitchButton::toggled,
            this, &ToolsPage::onLedToggled);

    // ===== 执行器状态反馈 =====
    Rte *rte = Rte::instance();
    connect(rte, &Rte::fanSpeedChanged, this, &ToolsPage::onFanCurSpeed);
    connect(rte, &Rte::windowPosChanged, this, &ToolsPage::onWindowCurPos);
}

// ==================== COM 发送 ====================

void ToolsPage::sendFanTarget(int percent)
{
    Com::instance()->sendSignal(SID_B_FAN_TARGET, (uint8_t)qBound(0, percent, 100));
}

void ToolsPage::sendWindowTarget(int percent)
{
    Com::instance()->sendSignal(SID_B_WINDOW_TARGET, (uint8_t)qBound(0, percent, 100));
}

void ToolsPage::sendLedState(bool on)
{
    Com::instance()->sendSignal(SID_B_LED_STATE, on ? 0x01u : 0x00u);
}

// ==================== 空 调 ====================

void ToolsPage::onAcToggled(bool on)
{
    m_acOn = on;
    ui->label_acStatus->setText(on ? "已开启" : "已关闭");
    ui->label_acStatus->setStyleSheet(
        on ? "color:#4caf50;font-weight:bold;" : "color:#999;");

    if (on) {
        if (m_fanLevel == 0)
            m_fanLevel = 80;
        sendFanTarget(m_fanLevel);
        ui->slider_fanSpeed->setValue(m_fanLevel);
    } else {
        sendFanTarget(0);
        // blockSignals 避免 setValue(0) 触发 onFanSliderChanged 把 m_fanLevel 清零
        ui->slider_fanSpeed->blockSignals(true);
        ui->slider_fanSpeed->setValue(0);
        ui->slider_fanSpeed->blockSignals(false);
        ui->label_fanValue->setText("0");
    }
    saveState();
}

void ToolsPage::onFanUp()
{
    if (!m_acOn) return;
    m_fanLevel = qMin(100, m_fanLevel + 10);
    applyFanLevel();
}

void ToolsPage::onFanDown()
{
    if (!m_acOn) return;
    m_fanLevel = qMax(0, m_fanLevel - 10);
    applyFanLevel();
}

void ToolsPage::onFanSliderChanged(int value)
{
    m_fanLevel = value;
    ui->label_fanValue->setText(QString::number(value));
    if (m_acOn)
        sendFanTarget(value);
}

void ToolsPage::applyFanLevel()
{
    sendFanTarget(m_fanLevel);
    ui->slider_fanSpeed->blockSignals(true);
    ui->slider_fanSpeed->setValue(m_fanLevel);
    ui->slider_fanSpeed->blockSignals(false);
    ui->label_fanValue->setText(QString::number(m_fanLevel));
}

// ==================== 车 窗 ====================

void ToolsPage::onWinPressed(int direction)
{
    m_winRepeatDir = direction;
    onWinRepeat(); // 立即执行一步
    m_winRepeatTimer->start();
}

void ToolsPage::onWinReleased()
{
    m_winRepeatTimer->stop();
    saveState();
}

void ToolsPage::onWinRepeat()
{
    m_windowPos = qBound(0, m_windowPos + m_winRepeatDir * 5, 100);
    sendWindowTarget(m_windowPos);
    ui->slider_window->blockSignals(true);
    ui->slider_window->setValue(m_windowPos);
    ui->slider_window->blockSignals(false);
    ui->label_windowPercent->setText(QString("%1%").arg(m_windowPos));
    updateWindowLabel();
}

void ToolsPage::onWinFullOpen()
{
    m_windowPos = 100;
    sendWindowTarget(m_windowPos);
    ui->slider_window->blockSignals(true);
    ui->slider_window->setValue(m_windowPos);
    ui->slider_window->blockSignals(false);
    ui->label_windowPercent->setText(QString("%1%").arg(m_windowPos));
    updateWindowLabel();
    saveState();
}

void ToolsPage::onWinFullClose()
{
    m_windowPos = 0;
    sendWindowTarget(m_windowPos);
    ui->slider_window->blockSignals(true);
    ui->slider_window->setValue(m_windowPos);
    ui->slider_window->blockSignals(false);
    ui->label_windowPercent->setText(QString("%1%").arg(m_windowPos));
    updateWindowLabel();
    saveState();
}

void ToolsPage::onWinSliderChanged(int value)
{
    m_windowPos = value;
    sendWindowTarget(value);
    ui->label_windowPercent->setText(QString("%1%").arg(value));
    updateWindowLabel();
}

void ToolsPage::updateWindowLabel()
{
    if (m_windowPos >= 95)
        ui->label_windowStatus->setText("已完全打开");
    else if (m_windowPos < 5)
        ui->label_windowStatus->setText("已关闭");
    else
        ui->label_windowStatus->setText(
            QString("打开中 %1%").arg(m_windowPos));
}

// ==================== 照 明 灯 ====================

void ToolsPage::onLedToggled(bool on)
{
    m_ledOn = on;
    sendLedState(on);
    ui->label_ledStatus->setText(on ? "已开启" : "已关闭");
    ui->icon_light->setIcon(on ? QIcon(":/images/images/灯泡_亮.png") : QIcon(":/images/images/灯泡_灭.png"));
    saveState();
}

// ==================== 执行器状态反馈 ====================

void ToolsPage::onFanCurSpeed(int percent)
{
    if (m_acOn) {
        ui->slider_fanSpeed->blockSignals(true);
        ui->slider_fanSpeed->setValue(percent);
        ui->slider_fanSpeed->blockSignals(false);
        ui->label_fanValue->setText(QString::number(percent));
    }
}

void ToolsPage::onWindowCurPos(int percent)
{
    m_windowPos = percent;
    ui->slider_window->blockSignals(true);
    ui->slider_window->setValue(percent);
    ui->slider_window->blockSignals(false);
    ui->label_windowPercent->setText(QString("%1%").arg(percent));
    updateWindowLabel();
}

// ==================== 状态持久化 ====================

void ToolsPage::loadState()
{
    ConfigManager *cfg = ConfigManager::instance();
    m_acOn             = cfg->value("tools/acOn", false).toBool();
    m_fanLevel         = cfg->value("tools/fanLevel", 0).toInt();
    m_ledOn            = cfg->value("tools/ledOn", false).toBool();
    m_windowPos        = cfg->value("tools/windowPos", 0).toInt();

    // 恢复 UI（blockSignals 避免 toggle 信号触发二次发送）
    ui->switch_ac->blockSignals(true);
    ui->switch_ac->setChecked(m_acOn);
    ui->switch_ac->blockSignals(false);
    ui->label_acStatus->setText(m_acOn ? "已开启" : "已关闭");
    ui->label_acStatus->setStyleSheet(m_acOn ? "color:#4caf50;font-weight:bold;" : "color:#999;");

    ui->switch_led->blockSignals(true);
    ui->switch_led->setChecked(m_ledOn);
    ui->switch_led->blockSignals(false);
    ui->label_ledStatus->setText(m_ledOn ? "已开启" : "已关闭");
    ui->icon_light->setIcon(m_ledOn ? QIcon(":/images/images/灯泡_亮.png") : QIcon(":/images/images/灯泡_灭.png"));

    ui->slider_fanSpeed->setValue(m_fanLevel);
    ui->label_fanValue->setText(QString::number(m_fanLevel));

    ui->slider_window->setValue(m_windowPos);
    ui->label_windowPercent->setText(QString("%1%").arg(m_windowPos));
    updateWindowLabel();

    // 发送初始状态
    sendLedState(m_ledOn);
    if (m_acOn)
        sendFanTarget(m_fanLevel);
    sendWindowTarget(m_windowPos);
}

void ToolsPage::saveState()
{
    ConfigManager *cfg = ConfigManager::instance();
    cfg->setValue("tools/acOn", m_acOn);
    cfg->setValue("tools/fanLevel", m_fanLevel);
    cfg->setValue("tools/ledOn", m_ledOn);
    cfg->setValue("tools/windowPos", m_windowPos);
}

// ==================== 工具子页面 ====================

void ToolsPage::onSerialTestClicked()
{
    if (m_serialAssistantPage == nullptr) {
        m_serialAssistantPage = new SerialAssistantPage(this);
        connect(m_serialAssistantPage, &SerialAssistantPage::backButtonClicked,
                this, [this]() {
                    ui->stackedWidget->setCurrentIndex(0);
                    ui->stackedWidget->removeWidget(m_serialAssistantPage);
                    m_serialAssistantPage->deleteLater();
                    m_serialAssistantPage = nullptr;
                });
        ui->stackedWidget->addWidget(m_serialAssistantPage);
    }
    ui->stackedWidget->setCurrentWidget(m_serialAssistantPage);
}

void ToolsPage::onCanbusTestClicked()
{
    if (m_udsDiagPage == nullptr) {
        m_udsDiagPage = new UdsDiagnosticPage(this);
        connect(m_udsDiagPage, &UdsDiagnosticPage::backButtonClicked,
                this, [this]() {
                    ui->stackedWidget->setCurrentIndex(0);
                    ui->stackedWidget->removeWidget(m_udsDiagPage);
                    m_udsDiagPage->deleteLater();
                    m_udsDiagPage = nullptr;
                });
        ui->stackedWidget->addWidget(m_udsDiagPage);
    }
    ui->stackedWidget->setCurrentWidget(m_udsDiagPage);
}

void ToolsPage::onGpioTestClicked()
{
    if (m_gpioTestPage == nullptr) {
        m_gpioTestPage = new GpioTestPage(this);
        connect(m_gpioTestPage, &GpioTestPage::backButtonClicked,
                this, [this]() {
                    ui->stackedWidget->setCurrentIndex(0);
                    ui->stackedWidget->removeWidget(m_gpioTestPage);
                    m_gpioTestPage->deleteLater();
                    m_gpioTestPage = nullptr;
                });
        ui->stackedWidget->addWidget(m_gpioTestPage);
    }
    ui->stackedWidget->setCurrentWidget(m_gpioTestPage);
}
