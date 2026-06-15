#ifndef CANMANAGER_H
#define CANMANAGER_H

#include <QObject>
#include <QtSerialBus/QCanBus>
#include <QtSerialBus/QCanBusDevice>
#include <QStringList>
#include <QByteArray>

class CanManager : public QObject
{
    Q_OBJECT

public:
    // CAN帧结构体
    struct CanFrame {
        quint32 id = 0;
        QByteArray data;
        bool isExtendedFrame = false;
        bool isRemoteFrame   = false;
        bool isValid         = false;
    };

    // 禁止拷贝和赋值
    CanManager(const CanManager &)            = delete;
    CanManager &operator=(const CanManager &) = delete;

    // 获取单例实例（线程安全）
    static CanManager *instance();

    // 设备管理
    QStringList scanDevices();
    bool connectDevice(const QString &deviceName, int bitrate);
    void disconnectDevice();
    bool isConnected() const;

    // 发送数据
    bool sendFrame(const CanFrame &frame);
    bool sendFrame(quint32 id, const QByteArray &data, bool isExtended = false, bool isRemote = false);

    // 工具方法 - 只负责数据转换，不负责格式化显示
    static CanFrame parseFrame(quint32 id, const QString &hexData, bool isExtended = false, bool isRemote = false);

signals:
    void statusChanged(const QString &status);
    void frameReceived(const CanFrame &frame);
    void frameSent(const CanFrame &frame);
    void errorOccurred(const QString &errorMsg);

private slots:
    void onFramesReceived();
    void onDeviceError(QCanBusDevice::CanBusError error);

private:
    explicit CanManager(QObject *parent = nullptr);
    ~CanManager() override;

    bool configureCanInterface(const QString &deviceName);
    CanFrame convertToCanFrame(const QCanBusFrame &qCanFrame);
    QCanBusFrame convertToQCanBusFrame(const CanFrame &frame);

    QCanBusDevice *m_canDevice = nullptr;
};

#endif // CANMANAGER_H