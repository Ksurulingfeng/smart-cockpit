#include "canAssistantPage.h"
#include "ui_canAssistantPage.h"
#include "config.h"
#include "iconhelper.h"
#include <QButtonGroup>
#include <QDateTime>
#include <QDebug>
#include <QMessageBox>
#include <QTextCursor>

// 常量定义
namespace
{
    constexpr int DEFAULT_CAN_ID                = 0x123;
    constexpr int DEFAULT_BITRATE_INDEX         = 5; // 250kbps
    constexpr int MIN_AUTO_SEND_INTERVAL_MS     = 10;
    constexpr int MAX_AUTO_SEND_INTERVAL_MS     = 10000;
    constexpr int DEFAULT_AUTO_SEND_INTERVAL_MS = 100;
}

CanAssistantPage::CanAssistantPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::CanAssistantPage)
    , m_canManager(CanManager::instance())
    , m_autoSendTimer(new QTimer(this))
    , m_rawDataMode(false)
{
    ui->setupUi(this);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    IconHelper::setupButton(ui->toolButton_back, Qt::black);

    // 如果主程序已经连接了 CAN 设备，同步 UI 状态并开始接收帧
    if (m_canManager->isConnected()) {
        ui->pushButton_open->setEnabled(false);
        ui->pushButton_close->setEnabled(true);
        ui->pushButton_send->setEnabled(true);
        ui->pushButton_refresh->setEnabled(false);
        ui->comboBox_canDevice->setEnabled(false);
        ui->comboBox_baudRate->setEnabled(false);
        ui->checkBox_autoSend->setEnabled(true);
        connectFrameSignals();
    }

    initUI();
    updateDeviceList();
}

CanAssistantPage::~CanAssistantPage()
{
    if (m_autoSendTimer->isActive()) {
        m_autoSendTimer->stop();
    }
    // 如果助手打开了设备，退出时恢复到默认 CAN 配置
    if (m_deviceOpenedByUs) {
        m_canManager->connectDevice(Config::CAN_DEVICE, CAN_BITRATE_HZ);
    }
    delete ui;
}

void CanAssistantPage::initUI()
{
    // ---------- 设置默认值 ----------
    ui->lineEdit_ID->setText(QString::number(DEFAULT_CAN_ID, 16));
    ui->lineEdit_data->setText("11 22 33 44 55 66 77 88");

    // 帧格式单选按钮组
    QButtonGroup *formatGroup = new QButtonGroup(this);
    formatGroup->addButton(ui->radioButton_std);
    formatGroup->addButton(ui->radioButton_ext);
    formatGroup->setExclusive(true);
    ui->radioButton_std->setChecked(true);

    // 帧类型单选按钮组
    QButtonGroup *typeGroup = new QButtonGroup(this);
    typeGroup->addButton(ui->radioButton_data);
    typeGroup->addButton(ui->radioButton_remote);
    typeGroup->setExclusive(true);
    ui->radioButton_data->setChecked(true);

    // 波特率下拉框
    ui->comboBox_baudRate->clear(); // UI 对象名保持原样，但内部使用正确变量名
    QList<int> rates = {10000, 20000, 50000, 100000, 125000,
                        250000, 500000, 800000, 1000000};
    for (int rate : rates) {
        ui->comboBox_baudRate->addItem(QString::number(rate), rate);
    }
    ui->comboBox_baudRate->setCurrentIndex(DEFAULT_BITRATE_INDEX);

    // 自动发送间隔
    ui->spinBox_autoSend->setRange(MIN_AUTO_SEND_INTERVAL_MS, MAX_AUTO_SEND_INTERVAL_MS);
    ui->spinBox_autoSend->setValue(DEFAULT_AUTO_SEND_INTERVAL_MS);
    m_autoSendTimer->setSingleShot(false);

    // 初始 UI 状态（未连接）
    ui->pushButton_open->setEnabled(true);
    ui->pushButton_close->setEnabled(false);
    ui->pushButton_send->setEnabled(false);
    ui->pushButton_refresh->setEnabled(true);
    ui->comboBox_canDevice->setEnabled(true);
    ui->comboBox_baudRate->setEnabled(true);
    ui->checkBox_autoSend->setEnabled(false);
    ui->spinBox_autoSend->setEnabled(false);

    // ---------- 显式连接信号槽 ----------
    // 按钮事件
    connect(ui->toolButton_back, &QToolButton::clicked, this, &CanAssistantPage::backButtonClicked);
    connect(ui->pushButton_open, &QPushButton::clicked, this, &CanAssistantPage::onOpenDevice);
    connect(ui->pushButton_close, &QPushButton::clicked, this, &CanAssistantPage::onCloseDevice);
    connect(ui->pushButton_refresh, &QPushButton::clicked, this, &CanAssistantPage::onRefreshDeviceList);
    connect(ui->pushButton_send, &QPushButton::clicked, this, &CanAssistantPage::onSendFrame);
    connect(ui->pushButton_clearReceive, &QPushButton::clicked, this, &CanAssistantPage::onClearReceive);
    connect(ui->pushButton_clearSend, &QPushButton::clicked, this, &CanAssistantPage::onClearSend);
    connect(ui->checkBox_autoSend, &QCheckBox::toggled, this, &CanAssistantPage::onAutoSendToggled);
    connect(ui->checkBox_rawData, &QCheckBox::toggled, this, [this](bool checked) {
        m_rawDataMode = checked;
        updateStatus(checked ? "原始数据显示模式已启用" : "格式化显示模式已启用");
    });

    // CAN 管理器信号（statusChanged/errorOccurred 始终连接，
    // frameReceived/frameSent 仅在设备打开后连接，见 connectFrameSignals/disconnectFrameSignals）
    connect(m_canManager, &CanManager::statusChanged, this, &CanAssistantPage::updateStatus);
    connect(m_canManager, &CanManager::errorOccurred, this, &CanAssistantPage::onErrorOccurred);

    // 自动发送定时器
    connect(m_autoSendTimer, &QTimer::timeout, this, &CanAssistantPage::onAutoSendTimeout);
}

void CanAssistantPage::updateDeviceList()
{
    ui->comboBox_canDevice->clear();
    QStringList devices = m_canManager->scanDevices();
    ui->comboBox_canDevice->addItems(devices);

    if (devices.isEmpty()) {
        updateStatus("未找到 CAN 设备");
    } else {
        updateStatus(QString("找到 %1 个 CAN 设备").arg(devices.size()));
    }
}

void CanAssistantPage::onOpenDevice()
{
    QString deviceName = ui->comboBox_canDevice->currentText();
    int baudRate       = ui->comboBox_baudRate->currentData().toInt();

    if (deviceName.isEmpty()) {
        updateStatus("请选择 CAN 设备");
        return;
    }

    if (m_canManager->connectDevice(deviceName, baudRate)) {
        m_deviceOpenedByUs = true;
        connectFrameSignals();
        ui->pushButton_open->setEnabled(false);
        ui->pushButton_close->setEnabled(true);
        ui->pushButton_send->setEnabled(true);
        ui->pushButton_refresh->setEnabled(false);
        ui->comboBox_canDevice->setEnabled(false);
        ui->comboBox_baudRate->setEnabled(false);
        ui->checkBox_autoSend->setEnabled(true);

        updateStatus(QString("成功连接到 %1，波特率 %2").arg(deviceName).arg(baudRate));
    }
}

void CanAssistantPage::onCloseDevice()
{
    if (ui->checkBox_autoSend->isChecked()) {
        ui->checkBox_autoSend->setChecked(false);
    }

    disconnectFrameSignals();

    // 仅当我们主动打开设备时才恢复默认连接
    if (m_deviceOpenedByUs) {
        m_canManager->connectDevice(Config::CAN_DEVICE, CAN_BITRATE_HZ);
        m_deviceOpenedByUs = false;
    }

    ui->pushButton_open->setEnabled(true);
    ui->pushButton_close->setEnabled(false);
    ui->pushButton_send->setEnabled(false);
    ui->pushButton_refresh->setEnabled(true);
    ui->comboBox_canDevice->setEnabled(true);
    ui->comboBox_baudRate->setEnabled(true);
    ui->checkBox_autoSend->setEnabled(false);
    ui->spinBox_autoSend->setEnabled(false);

    updateStatus("CAN 设备已恢复到默认配置");
}

void CanAssistantPage::onSendFrame()
{
    if (!m_canManager->isConnected()) {
        updateStatus("设备未连接，无法发送数据");
        return;
    }

    bool ok;
    quint32 id = ui->lineEdit_ID->text().toUInt(&ok, 16);
    if (!ok) {
        updateStatus("ID 格式错误，请使用十六进制格式（如 123）");
        return;
    }

    QString dataStr            = ui->lineEdit_data->text();
    bool isExtended            = ui->radioButton_ext->isChecked();
    bool isRemote              = ui->radioButton_remote->isChecked();

    CanManager::CanFrame frame = CanManager::parseFrame(id, dataStr, isExtended, isRemote);
    if (!frame.isValid) {
        updateStatus("帧无效：ID 超出范围或数据长度超过 8 字节");
        return;
    }

    if (m_canManager->sendFrame(frame)) {
        updateStatus(QString("发送成功 - ID:0x%1, DLC:%2")
                         .arg(id, 0, 16)
                         .arg(frame.data.size()));
    }
}

void CanAssistantPage::onClearReceive()
{
    ui->textEdit_receive->clear();
    updateStatus("接收区已清空");
}

void CanAssistantPage::onClearSend()
{
    ui->lineEdit_data->clear();
    updateStatus("发送区已清空");
}

void CanAssistantPage::onRefreshDeviceList()
{
    updateDeviceList();
}

void CanAssistantPage::onAutoSendToggled(bool checked)
{
    if (checked) {
        if (!m_canManager->isConnected()) {
            updateStatus("设备未连接，无法启动自动发送");
            ui->checkBox_autoSend->setChecked(false);
            return;
        }

        bool ok;
        quint32 id = ui->lineEdit_ID->text().toUInt(&ok, 16);
        if (!ok) {
            updateStatus("ID 格式错误，无法启动自动发送");
            ui->checkBox_autoSend->setChecked(false);
            return;
        }

        QString dataStr = ui->lineEdit_data->text();
        bool isExtended = ui->radioButton_ext->isChecked();
        bool isRemote   = ui->radioButton_remote->isChecked();

        m_autoSendFrame = CanManager::parseFrame(id, dataStr, isExtended, isRemote);
        if (!m_autoSendFrame.isValid) {
            updateStatus("自动发送帧无效：ID 超出范围或数据长度超过 8 字节");
            ui->checkBox_autoSend->setChecked(false);
            return;
        }

        int interval = ui->spinBox_autoSend->value();
        m_autoSendTimer->start(interval);
        ui->spinBox_autoSend->setEnabled(false); // 自动发送时禁止修改间隔
        updateStatus(QString("自动发送已启动，间隔 %1 ms").arg(interval));
    } else {
        m_autoSendTimer->stop();
        ui->spinBox_autoSend->setEnabled(true); // 停止后允许修改间隔
        updateStatus("自动发送已停止");
    }
}

void CanAssistantPage::onAutoSendTimeout()
{
    if (!m_canManager->isConnected()) {
        updateStatus("设备已断开，自动发送停止");
        ui->checkBox_autoSend->setChecked(false);
        return;
    }

    if (!m_autoSendFrame.isValid) {
        updateStatus("自动发送帧无效，停止发送");
        ui->checkBox_autoSend->setChecked(false);
        return;
    }

    m_canManager->sendFrame(m_autoSendFrame);
}

void CanAssistantPage::onFrameReceived(const CanManager::CanFrame &frame)
{
    QString time     = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString frameStr = formatFrameForDisplay(frame);
    appendReceiveData(QString("[%1] 接收: %2").arg(time).arg(frameStr));
}

void CanAssistantPage::onFrameSent(const CanManager::CanFrame &frame)
{
    QString time     = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString frameStr = formatFrameForDisplay(frame);
    appendReceiveData(QString("[%1] 发送: %2").arg(time).arg(frameStr));
}

void CanAssistantPage::onErrorOccurred(const QString &errorMsg)
{
    QMessageBox::warning(this, "CAN 错误", errorMsg);
    updateStatus("错误: " + errorMsg);
}

void CanAssistantPage::updateStatus(const QString &status)
{
    qDebug() << "[CAN助手]" << status;
    ui->label_canAssistant->setToolTip(status);
    ui->label_canAssistant->setToolTipDuration(3000);
}

void CanAssistantPage::appendReceiveData(const QString &data)
{
    ui->textEdit_receive->append(data);
    // 自动滚动到底部
    QTextCursor cursor = ui->textEdit_receive->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->textEdit_receive->setTextCursor(cursor);
}

void CanAssistantPage::connectFrameSignals()
{
    connect(m_canManager, &CanManager::frameReceived, this, &CanAssistantPage::onFrameReceived);
    connect(m_canManager, &CanManager::frameSent, this, &CanAssistantPage::onFrameSent);
}

void CanAssistantPage::disconnectFrameSignals()
{
    disconnect(m_canManager, &CanManager::frameReceived, this, &CanAssistantPage::onFrameReceived);
    disconnect(m_canManager, &CanManager::frameSent, this, &CanAssistantPage::onFrameSent);
}

QString CanAssistantPage::formatFrameForDisplay(const CanManager::CanFrame &frame) const
{
    if (!frame.isValid) {
        return "无效帧";
    }

    if (m_rawDataMode) {
        // 原始模式：ID(8位十六进制) + 长度(2位十进制) + 数据(2位十六进制)
        QString result = QString("%1 ").arg(frame.id, 8, 16, QChar('0')).toUpper();
        result += QString("%1 ").arg(frame.data.size(), 2, 10, QChar('0'));
        for (int i = 0; i < frame.data.size(); ++i) {
            result += QString("%1 ").arg(static_cast<quint8>(frame.data[i]), 2, 16, QChar('0')).toUpper();
        }
        return result.trimmed();
    } else {
        // 格式化模式：易读格式
        QString result = QString("[ID:%1] ").arg(QString::number(frame.id, 16).toUpper());
        result += QString("[%1] ").arg(frame.data.size());
        if (!frame.data.isEmpty()) {
            result += "[数据: ";
            for (int i = 0; i < frame.data.size(); ++i) {
                result += QString("%1 ").arg(static_cast<quint8>(frame.data[i]), 2, 16, QChar('0')).toUpper();
            }
            result += "]";
        }
        if (frame.isRemoteFrame) result += " [远程帧]";
        if (frame.isExtendedFrame) result += " [扩展帧]";
        return result.trimmed();
    }
}