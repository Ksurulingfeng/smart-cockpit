/**
 * @file    cantp.h
 * @brief   AUTOSAR CanTp — ISO 15765-2 CAN 多帧传输层
 *
 * 支持 Single Frame (≤7 字节) / First Frame / Consecutive Frame 的分帧与重组。
 * UDS 诊断请求→响应均通过此层传输 >8 字节的 PDU。
 */

#ifndef CANTP_H
#define CANTP_H

#include <QObject>
#include <QByteArray>
#include <QTimer>
#include <cstdint>

class CanTp : public QObject
{
    Q_OBJECT
public:
    static CanTp *instance();
    CanTp(const CanTp &) = delete;
    CanTp &operator=(const CanTp &) = delete;

    // 配置 UDS CAN ID
    void configure(uint32_t requestId = 0x7E0, uint32_t responseId = 0x7E8);

    // 接收帧（由 PduR 调用）
    void receiveFrame(uint32_t canId, const uint8_t *data, uint8_t len);

    // 发送完整消息（由 DCM 调用，自动分帧）
    bool sendMessage(const QByteArray &data);

signals:
    // 多帧重组完成 → DCM
    void messageReceived(const QByteArray &data);
    // 发送帧请求 → PduR → CanManager
    void sendFrameRequest(uint32_t canId, const QByteArray &data);
    // 错误通知
    void error(const QString &msg);

private slots:
    void onNAsTimeout();   // N_As: 发送方等待 FC 超时
    void onNCrTimeout();   // N_Cr: 接收方等待 CF 超时

private:
    explicit CanTp(QObject *parent = nullptr);
    ~CanTp() override = default;

    enum RxState { RX_IDLE, RX_WAIT_CF };
    enum TxState { TX_IDLE, TX_WAIT_FC, TX_SENDING_CF };

    // 接收处理
    void handleSingleFrame(const uint8_t *data, uint8_t len);
    void handleFirstFrame(const uint8_t *data, uint8_t len);
    void handleConsecutiveFrame(const uint8_t *data, uint8_t len);
    void sendFlowControl(uint8_t flowStatus, uint8_t blockSize, uint8_t stMin);

    // 发送处理
    void handleFlowControlFrame(const uint8_t *data, uint8_t len);
    void sendFirstFrame();
    void sendNextConsecutiveFrame();

    QByteArray m_rxBuffer;
    uint32_t   m_rxTotalLen = 0;
    uint8_t    m_rxSeqNum   = 0;
    RxState    m_rxState    = RX_IDLE;

    QByteArray m_txBuffer;
    uint32_t   m_txBytesSent = 0;
    uint8_t    m_txSeqNum    = 1;
    uint8_t    m_txBlockSize = 0;
    uint8_t    m_txStMin     = 0;
    TxState    m_txState     = TX_IDLE;

    uint32_t   m_requestId  = 0x7E0;
    uint32_t   m_responseId = 0x7E8;

    QTimer *m_nAsTimer = nullptr;  // N_As: 发送方等待 FC 超时 (1s)
    QTimer *m_nCrTimer = nullptr;  // N_Cr: 接收方等待 CF 超时 (1s)
};

#endif // CANTP_H
