/**
 * @file    dcm.h
 * @brief   AUTOSAR DCM — 诊断通信管理 (UDS on CAN)
 *
 * 处理 UDS 诊断请求，生成 UDS 诊断响应。
 * 服务号: 0x10 会话控制 / 0x11 ECU复位 / 0x22 读数据 / 0x2E 写数据
 *         0x14 清除DTC / 0x19 读DTC / 0x3E 诊断会话保持
 */

#ifndef DCM_H
#define DCM_H

#include <QObject>
#include <QByteArray>
#include <QTimer>
#include <cstdint>

class Dcm : public QObject
{
    Q_OBJECT
public:
    static Dcm *instance();
    Dcm(const Dcm &) = delete;
    Dcm &operator=(const Dcm &) = delete;

    // 由 CanTp::messageReceived 调用
    void processUdsMessage(const QByteArray &data);

signals:
    void sendUdsResponse(const QByteArray &data);  // → CanTp::sendMessage
    void sessionChanged(uint8_t session);

private slots:
    void onSessionTimeout();

private:
    explicit Dcm(QObject *parent = nullptr);
    ~Dcm() override = default;

    // UDS 服务处理
    void handleDiagSessionControl(const QByteArray &data);    // 0x10
    void handleEcuReset(const QByteArray &data);              // 0x11
    void handleReadDataByIdentifier(const QByteArray &data);  // 0x22
    void handleWriteDataByIdentifier(const QByteArray &data); // 0x2E
    void handleTesterPresent(const QByteArray &data);         // 0x3E
    void handleReadDtcInfo(const QByteArray &data);           // 0x19
    void handleClearDtcInfo(const QByteArray &data);          // 0x14

    void sendPositiveResponse(uint8_t sid, const QByteArray &data);
    void sendNegativeResponse(uint8_t sid, uint8_t nrc);

    uint8_t m_currentSession = 0x01;    // 0x01=默认, 0x02=编程, 0x03=扩展
    QTimer *m_s3Timer = nullptr;        // 非默认会话超时 5s
};

#endif // DCM_H
