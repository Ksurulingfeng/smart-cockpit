#include "canmanager.h"
#include <QDir>
#include <QProcess>
#include <QDebug>

CanManager *CanManager::instance()
{
    static CanManager instance;
    return &instance;
}

CanManager::CanManager(QObject *parent)
    : QObject(parent)
{
}

CanManager::~CanManager()
{
    disconnectDevice();
}

QStringList CanManager::scanDevices()
{
    QStringList devices;

    if (QCanBus::instance()->plugins().contains("socketcan")) {
        const QList<QCanBusDeviceInfo> interfaces = QCanBus::instance()->availableDevices("socketcan");
        for (const QCanBusDeviceInfo &info : interfaces) {
            devices.append(info.name());
        }
    }

    return devices;
}

bool CanManager::connectDevice(const QString &deviceName, int bitrate)
{
    if (m_canDevice) {
        disconnectDevice();
    }

    // 先关闭接口以便后续配置
    if (!configureCanInterface(deviceName)) {
        emit errorOccurred("CAN接口配置失败");
        return false;
    }

    QString errorString;
    m_canDevice = QCanBus::instance()->createDevice("socketcan", deviceName, &errorString);
    if (!m_canDevice) {
        emit errorOccurred(QString("创建CAN设备失败: %1").arg(errorString));
        return false;
    }

    connect(m_canDevice, &QCanBusDevice::framesReceived, this, &CanManager::onFramesReceived);
    connect(m_canDevice, &QCanBusDevice::errorOccurred, this, &CanManager::onDeviceError);

    // 由 Qt 配置 bitrate（此时接口为 DOWN，netlink 可正常写入）
    if (!deviceName.startsWith("vcan"))
        m_canDevice->setConfigurationParameter(QCanBusDevice::BitRateKey, QVariant(bitrate));

    if (!m_canDevice->connectDevice()) {
        QString errorMsg = QString("连接失败: %1").arg(m_canDevice->errorString());
        delete m_canDevice;
        m_canDevice = nullptr;
        emit errorOccurred(errorMsg);
        return false;
    }

    // Qt 连接完成后启用接口
    if (!deviceName.startsWith("vcan")) {
        QProcess process;
        process.start("ip", QStringList() << "link" << "set" << "up" << deviceName);
        process.waitForFinished(1000);
    }

    return true;
}

void CanManager::disconnectDevice()
{
    if (m_canDevice) {
        if (m_canDevice->state() == QCanBusDevice::ConnectedState) {
            m_canDevice->disconnectDevice();
        }
        delete m_canDevice;
        m_canDevice = nullptr;
    }
}

bool CanManager::isConnected() const
{
    return m_canDevice && (m_canDevice->state() == QCanBusDevice::ConnectedState);
}

bool CanManager::sendFrame(const CanFrame &frame)
{
    if (!isConnected() || !frame.isValid) {
        emit errorOccurred("发送失败：设备未连接或帧无效");
        return false;
    }

    QCanBusFrame qFrame = convertToQCanBusFrame(frame);
    if (m_canDevice->writeFrame(qFrame)) {
        emit frameSent(frame);
        return true;
    } else {
        emit errorOccurred(QString("发送失败: %1").arg(m_canDevice->errorString()));
        return false;
    }
}

bool CanManager::sendFrame(quint32 id, const QByteArray &data, bool isExtended, bool isRemote)
{
    CanFrame frame;
    frame.id              = id;
    frame.data            = data;
    frame.isExtendedFrame = isExtended;
    frame.isRemoteFrame   = isRemote;
    frame.isValid         = true;

    return sendFrame(frame);
}

CanManager::CanFrame CanManager::parseFrame(quint32 id, const QString &hexData, bool isExtended, bool isRemote)
{
    CanFrame frame;
    frame.id              = id;
    frame.isExtendedFrame = isExtended;
    frame.isRemoteFrame   = isRemote;

    QString cleanData     = hexData.trimmed().remove(' ');
    if (!cleanData.isEmpty()) {
        frame.data = QByteArray::fromHex(cleanData.toLatin1());
    }

    frame.isValid = (id <= (isExtended ? 0x1FFFFFFF : 0x7FF)) && (frame.data.size() <= 8);
    return frame;
}

void CanManager::onFramesReceived()
{
    if (!m_canDevice) return;

    while (m_canDevice->framesAvailable()) {
        QCanBusFrame qFrame = m_canDevice->readFrame();
        CanFrame frame      = convertToCanFrame(qFrame);
        emit frameReceived(frame);
    }
}

void CanManager::onDeviceError(QCanBusDevice::CanBusError error)
{
    Q_UNUSED(error);
    if (m_canDevice) {
        emit errorOccurred(QString("CAN设备错误: %1").arg(m_canDevice->errorString()));
    } else {
        emit errorOccurred("CAN设备错误：设备指针为空");
    }
}

bool CanManager::configureCanInterface(const QString &deviceName)
{
    QProcess process;

    // 关闭接口，确保后续 netlink 配置不会遇到 EBUSY
    process.start("ip", QStringList() << "link" << "set" << deviceName << "down");
    process.waitForFinished(1000);

    if (!deviceName.startsWith("vcan")) {
        // 仅设置接口类型为 CAN，不设 bitrate（由 Qt 配置），不启用接口
        process.start("ip", QStringList() << "link" << "set" << deviceName << "type" << "can");
        process.waitForFinished(500);
        if (process.exitCode() != 0) {
            emit errorOccurred("CAN类型配置失败: " + process.readAllStandardError());
            return false;
        }
    }

    return true;
}

CanManager::CanFrame CanManager::convertToCanFrame(const QCanBusFrame &qCanFrame)
{
    CanFrame frame;
    frame.id              = qCanFrame.frameId();
    frame.data            = qCanFrame.payload();
    frame.isExtendedFrame = qCanFrame.hasExtendedFrameFormat();
    frame.isRemoteFrame   = (qCanFrame.frameType() == QCanBusFrame::RemoteRequestFrame);
    frame.isValid         = qCanFrame.isValid();

    return frame;
}

QCanBusFrame CanManager::convertToQCanBusFrame(const CanFrame &frame)
{
    QCanBusFrame qFrame(frame.id, frame.data);
    if (frame.isExtendedFrame) {
        qFrame.setExtendedFrameFormat(true);
    }
    if (frame.isRemoteFrame) {
        qFrame.setFrameType(QCanBusFrame::RemoteRequestFrame);
    }

    return qFrame;
}