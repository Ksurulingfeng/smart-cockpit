#include "gpioTestPage.h"
#include "ui_gpioTestPage.h"
#include "sysfsled.h"
#include "iconhelper.h"
#include <QDebug>
#include <QIcon>
#include <QKeyEvent>

namespace
{
    const QString LED_BRIGHTNESS_PATH  = "/sys/class/leds/sys-led/brightness";
    const QString BEEP_BRIGHTNESS_PATH = "/sys/class/leds/beep/brightness";
}

GpioTestPage::GpioTestPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::GpioTestPage)
    , m_ledFlashTimer(new QTimer(this))
    , m_keyCount(0)
    , m_currentLedState(false)
    , m_beepContinuous(false)
    , m_led(nullptr)
    , m_beep(nullptr)
{
    ui->setupUi(this);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    IconHelper::setupButton(ui->toolButton_back, Qt::black);

    // 创建 LED 和蜂鸣器控制对象（RAII，父对象销毁时自动释放）
    m_led  = new SysfsLed(LED_BRIGHTNESS_PATH, this);
    m_beep = new SysfsLed(BEEP_BRIGHTNESS_PATH, this);

    // 连接错误信号（可选）
    connect(m_led, &SysfsLed::errorOccurred, this, &GpioTestPage::onLedError);
    connect(m_beep, &SysfsLed::errorOccurred, this, &GpioTestPage::onBeepError);

    initUI();

    // 默认关闭心跳模式，LED 关闭
    setLedTrigger("none");
    setLedState(false);
    ui->pushButton_toggle->setChecked(false);
    ui->checkBox_heartMode->setChecked(false);
    ui->checkBox_flash->setChecked(false);
    ui->spinBox_flashTime->setValue(500);
    ui->spinBox_triggerTime->setValue(200);

    setFocusPolicy(Qt::StrongFocus);
}

GpioTestPage::~GpioTestPage()
{
    if (m_ledFlashTimer->isActive())
        m_ledFlashTimer->stop();
    delete ui;
    // m_led 和 m_beep 由 Qt 父子关系自动删除
}

void GpioTestPage::initUI()
{
    updateLedIcon();
    updateBeepIcon();

    connect(ui->pushButton_toggle, &QPushButton::toggled, this, &GpioTestPage::onLedToggle);
    connect(ui->checkBox_heartMode, &QCheckBox::toggled, this, &GpioTestPage::onHeartModeToggled);
    connect(ui->checkBox_flash, &QCheckBox::toggled, this, &GpioTestPage::onFlashToggled);
    connect(m_ledFlashTimer, &QTimer::timeout, this, &GpioTestPage::onFlashTimeout);
    connect(ui->pushButton_toggle_2, &QPushButton::toggled, this, &GpioTestPage::onBeepToggle);
    connect(ui->pushButton_trigger, &QPushButton::clicked, this, &GpioTestPage::onBeepTrigger);
    connect(ui->toolButton_back, &QToolButton::clicked, this, &GpioTestPage::backButtonClicked);
    connect(ui->pushButton_resetNum, &QPushButton::clicked, this, &GpioTestPage::onClearKeyPressedNum);
}

void GpioTestPage::setLedState(bool on)
{
    if (!m_led->isValid()) {
        updateStatus("LED 设备不可用");
        return;
    }
    m_led->setState(on);
    m_currentLedState = on;
    updateLedIcon();
}

void GpioTestPage::setLedTrigger(const QString &trigger)
{
    if (!m_led->isValid()) return;
    m_led->setTrigger(trigger);
}

void GpioTestPage::setBeepState(bool on)
{
    if (!m_beep->isValid()) {
        updateStatus("蜂鸣器设备不可用");
        return;
    }
    m_beep->setState(on);
    m_beepContinuous = on;
    updateBeepIcon();
}

void GpioTestPage::updateLedIcon()
{
    if (m_currentLedState)
        ui->toolButton_DS0->setIcon(QIcon(":/images/images/灯泡_亮.png"));
    else
        ui->toolButton_DS0->setIcon(QIcon(":/images/images/灯泡_灭.png"));
}

void GpioTestPage::updateBeepIcon()
{
    if (m_beepContinuous)
        ui->toolButton_BEEP->setIcon(QIcon(":/images/images/铃声_开.png"));
    else
        ui->toolButton_BEEP->setIcon(QIcon(":/images/images/铃声_关.png"));
}

void GpioTestPage::updateStatus(const QString &msg)
{
    qDebug() << "[GPIO Test]" << msg;
    emit statusUpdated(msg);
}

void GpioTestPage::onLedToggle(bool checked)
{
    if (ui->checkBox_heartMode->isChecked() || ui->checkBox_flash->isChecked()) {
        ui->pushButton_toggle->setChecked(!checked);
        return;
    }
    setLedState(checked);
    ui->pushButton_toggle->setText(checked ? "关闭" : "开启");
}

void GpioTestPage::onHeartModeToggled(bool checked)
{
    if (checked) {
        setLedTrigger("heartbeat");
        ui->checkBox_flash->setEnabled(false);
        ui->pushButton_toggle->setEnabled(false);
        if (m_ledFlashTimer->isActive())
            m_ledFlashTimer->stop();
    } else {
        setLedTrigger("none");
        ui->checkBox_flash->setEnabled(true);
        ui->pushButton_toggle->setEnabled(true);
        if (ui->checkBox_flash->isChecked()) {
            int interval = ui->spinBox_flashTime->value();
            m_ledFlashTimer->start(interval);
        } else {
            bool ledOn = ui->pushButton_toggle->isChecked();
            setLedState(ledOn);
        }
    }
}

void GpioTestPage::onFlashToggled(bool checked)
{
    if (ui->checkBox_heartMode->isChecked()) {
        ui->checkBox_flash->setChecked(false);
        return;
    }
    if (checked) {
        ui->pushButton_toggle->setEnabled(false);
        int interval = ui->spinBox_flashTime->value();
        m_ledFlashTimer->start(interval);
        setLedState(false);
    } else {
        m_ledFlashTimer->stop();
        ui->pushButton_toggle->setEnabled(true);
        bool manualOn = ui->pushButton_toggle->isChecked();
        setLedState(manualOn);
    }
}

void GpioTestPage::onFlashTimeout()
{
    if (!ui->checkBox_flash->isChecked() || ui->checkBox_heartMode->isChecked()) {
        if (m_ledFlashTimer->isActive())
            m_ledFlashTimer->stop();
        return;
    }
    setLedState(!m_currentLedState);
}

void GpioTestPage::onBeepToggle(bool checked)
{
    setBeepState(checked);
    ui->pushButton_toggle_2->setText(checked ? "关闭" : "开启");
}

void GpioTestPage::onBeepTrigger()
{
    if (!m_beep->isValid()) {
        updateStatus("蜂鸣器不可用，无法触发");
        return;
    }

    int duration       = ui->spinBox_triggerTime->value();
    bool originalState = m_beep->state();

    setBeepState(true);
    QTimer::singleShot(duration, this, [this, originalState]() {
        if (ui->pushButton_toggle_2->isChecked())
            return;
        setBeepState(originalState);
    });
}

void GpioTestPage::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_VolumeDown) {
        m_keyCount++;
        ui->label_keyTriggerNum->setText(QString("触发次数：%1").arg(m_keyCount));
        event->accept();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void GpioTestPage::onClearKeyPressedNum()
{
    m_keyCount = 0;
    ui->label_keyTriggerNum->setText("触发次数：0");
}

void GpioTestPage::onLedError(const QString &err)
{
    updateStatus("LED 错误: " + err);
}

void GpioTestPage::onBeepError(const QString &err)
{
    updateStatus("蜂鸣器错误: " + err);
}