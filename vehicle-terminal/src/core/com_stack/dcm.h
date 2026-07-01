/**
 * @file    dcm.h
 * @brief   AUTOSAR DCM — 诊断通信管理 (UDS on CAN, 裸帧)
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

    // 选择目标节点（切换 CAN ID）
    void setTargetNode(int nodeId);  // 0=NodeA (0x7E0/0x7E8), 1=NodeB (0x7E1/0x7E9)
    int  targetNode() const { return m_targetNode; }

    // 由 PduR 调用（接收裸 UDS 帧）
    void processUdsMessage(const QByteArray &data);

    // 发送 UDS 请求（裸帧，发给当前目标节点）
    void sendUdsRequest(const QByteArray &data);

signals:
    void sendRawFrame(uint32_t canId, const QByteArray &data);  // → PduR → CanManager
    void sendUdsResponse(const QByteArray &data);               // → UI 日志
    void sessionChanged(uint8_t session);

private slots:
    void onSessionTimeout();

private:
    explicit Dcm(QObject *parent = nullptr);
    ~Dcm() override = default;

    void handleDiagSessionControl(const QByteArray &data);
    void handleEcuReset(const QByteArray &data);
    void handleReadDataByIdentifier(const QByteArray &data);
    void handleWriteDataByIdentifier(const QByteArray &data);
    void handleTesterPresent(const QByteArray &data);
    void handleReadDtcInfo(const QByteArray &data);
    void handleClearDtcInfo(const QByteArray &data);

    void sendPositiveResponse(uint8_t sid, const QByteArray &data);
    void sendNegativeResponse(uint8_t sid, uint8_t nrc);

    uint32_t m_reqId  = 0x7E0;     // 当前目标节点的 UDS 请求 CAN ID
    uint32_t m_respId = 0x7E8;     // 当前目标节点的 UDS 响应 CAN ID
    int      m_targetNode = 0;

    uint8_t m_currentSession = 0x01;
    QTimer *m_s3Timer = nullptr;
};

#endif // DCM_H
