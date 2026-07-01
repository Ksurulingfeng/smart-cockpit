/**
 * @file    cantp.cpp
 * @brief   CanTp 实现 — ISO 15765-2 多帧传输
 *
 * PCI (Protocol Control Information) 编码:
 *   byte0 高4位: 0=SF, 1=FF, 2=CF, 3=FC
 *   SF: byte0 低4位 = 数据长度 (0-7)
 *   FF: byte0 低4位 + byte1 = 12 位总长度 (8-4095)
 *   CF: byte0 低4位 = 序号 (1-15, 循环)
 *   FC: byte0 = 0x30(CTS) / 0x31(WT) / 0x32(OVFLW)
 */

#include "cantp.h"
#include <QDebug>

#define PCI_TYPE_MASK  0xF0
#define PCI_SF         0x00
#define PCI_FF         0x10
#define PCI_CF         0x20
#define PCI_FC         0x30

#define FC_CTS         0x00
#define FC_WT          0x01
#define FC_OVFLW       0x02

// 定时器常量
static constexpr int N_AS_MS = 1000;  // 发送方等待 FC 超时
static constexpr int N_CR_MS = 1000;  // 接收方等待 CF 超时
static constexpr int N_BS    = 8;     // 块大小（默认一次发8帧）
static constexpr int ST_MIN  = 0;     // 最小帧间隔 (ms)

CanTp *CanTp::instance()
{
    static CanTp inst;
    return &inst;
}

CanTp::CanTp(QObject *parent)
    : QObject(parent)
{
    m_nAsTimer = new QTimer(this);
    m_nAsTimer->setSingleShot(true);
    connect(m_nAsTimer, &QTimer::timeout, this, &CanTp::onNAsTimeout);

    m_nCrTimer = new QTimer(this);
    m_nCrTimer->setSingleShot(true);
    connect(m_nCrTimer, &QTimer::timeout, this, &CanTp::onNCrTimeout);
}

void CanTp::configure(uint32_t requestId, uint32_t responseId)
{
    m_requestId  = requestId;
    m_responseId = responseId;
}

// ==================== 接收入口 ====================

void CanTp::receiveFrame(uint32_t canId, const uint8_t *data, uint8_t len)
{
    if (len < 1) return;

    uint8_t pci = data[0];
    uint8_t type = pci & PCI_TYPE_MASK;

    switch (type) {
    case PCI_SF:
        handleSingleFrame(data, len);
        break;
    case PCI_FF:
        handleFirstFrame(data, len);
        break;
    case PCI_CF:
        handleConsecutiveFrame(data, len);
        break;
    case PCI_FC:
        handleFlowControlFrame(data, len);
        break;
    default:
        emit error("CanTp: unknown PCI type");
        break;
    }
}

// ==================== 接收: Single Frame ====================

void CanTp::handleSingleFrame(const uint8_t *data, uint8_t len)
{
    uint8_t sfLen = data[0] & 0x0F;
    if (sfLen == 0 || (uint8_t)(sfLen + 1) != len) {
        emit error("CanTp: SF length mismatch");
        return;
    }
    // SF: byte0 是 PCI, byte1+ 是数据
    QByteArray msg((const char *)(data + 1), sfLen);
    emit messageReceived(msg);
}

// ==================== 接收: First Frame → 回复 Flow Control ====================

void CanTp::handleFirstFrame(const uint8_t *data, uint8_t len)
{
    if (len < 2) {
        emit error("CanTp: FF too short");
        return;
    }

    m_rxTotalLen = ((uint32_t)(data[0] & 0x0F) << 8) | (uint32_t)data[1];
    if (m_rxTotalLen < 8 || m_rxTotalLen > 4095) {
        emit error("CanTp: FF invalid length");
        return;
    }

    m_rxBuffer.clear();
    // FF 数据从 byte2 开始
    int ffDataLen = len - 2;
    m_rxBuffer.append((const char *)(data + 2), ffDataLen);
    m_rxSeqNum = 1;
    m_rxState  = RX_WAIT_CF;

    // 发送 CTS (Continue To Send), blockSize=N_BS, STmin=0
    sendFlowControl(FC_CTS, N_BS, ST_MIN);
}

// ==================== 接收: Consecutive Frame → 累积数据 ====================

void CanTp::handleConsecutiveFrame(const uint8_t *data, uint8_t len)
{
    if (m_rxState != RX_WAIT_CF) {
        emit error("CanTp: unexpected CF");
        return;
    }

    uint8_t seqNum = data[0] & 0x0F;
    if (seqNum != m_rxSeqNum) {
        emit error(QString("CanTp: CF seq mismatch: expected %1 got %2")
                           .arg(m_rxSeqNum).arg(seqNum));
        m_rxState = RX_IDLE;
        return;
    }

    // CF 数据从 byte1 开始
    m_rxBuffer.append((const char *)(data + 1), len - 1);
    m_rxSeqNum = (m_rxSeqNum + 1) & 0x0F;
    if (m_rxSeqNum == 0) m_rxSeqNum = 1;

    m_nCrTimer->start(N_CR_MS);

    if (m_rxBuffer.size() >= (int)m_rxTotalLen) {
        // 接收完成
        m_rxState = RX_IDLE;
        m_nCrTimer->stop();
        emit messageReceived(m_rxBuffer.left(m_rxTotalLen));
    }
}

void CanTp::sendFlowControl(uint8_t flowStatus, uint8_t blockSize, uint8_t stMin)
{
    QByteArray fc;
    fc.append((char)(PCI_FC | flowStatus));
    fc.append((char)blockSize);
    fc.append((char)stMin);
    // 填充到 8 字节
    while (fc.size() < 8) fc.append('\0');

    emit sendFrameRequest(m_responseId, fc);
}

// ==================== 发送: 完整消息 → 分包 ====================

bool CanTp::sendMessage(const QByteArray &data)
{
    if (data.isEmpty() || data.size() > 4095) {
        emit error("CanTp: invalid message length");
        return false;
    }

    if (m_txState != TX_IDLE) {
        emit error("CanTp: TX busy");
        return false;
    }

    m_txBuffer     = data;
    m_txBytesSent  = 0;
    m_txSeqNum     = 1;

    if (data.size() <= 7) {
        // ---- Single Frame ----
        QByteArray sf;
        sf.append((char)(PCI_SF | (uint8_t)data.size()));
        sf.append(data);
        while (sf.size() < 8) sf.append('\0');
        emit sendFrameRequest(m_requestId, sf);
        // SF 无需回复，直接完成
    } else {
        // ---- First Frame + Consecutive Frames ----
        m_txState = TX_WAIT_FC;
        sendFirstFrame();
        m_nAsTimer->start(N_AS_MS);
    }

    return true;
}

void CanTp::sendFirstFrame()
{
    uint32_t totalLen = (uint32_t)m_txBuffer.size();
    QByteArray ff;
    ff.append((char)(PCI_FF | (uint8_t)((totalLen >> 8) & 0x0F)));
    ff.append((char)(totalLen & 0xFF));
    // FF payload: 前 6 字节
    int ffPayload = qMin(6, m_txBuffer.size());
    ff.append(m_txBuffer.left(ffPayload));
    while (ff.size() < 8) ff.append('\0');

    m_txBytesSent = ffPayload;
    emit sendFrameRequest(m_requestId, ff);
}

// ==================== 发送: 处理 Flow Control ====================

void CanTp::handleFlowControlFrame(const uint8_t *data, uint8_t len)
{
    if (m_txState != TX_WAIT_FC) return;

    uint8_t flowStatus = data[0] & 0x0F;
    if (flowStatus == FC_CTS) {
        m_nAsTimer->stop();
        m_txBlockSize = (len >= 2) ? data[1] : 1;
        m_txStMin     = (len >= 3) ? data[2] : 0;
        if (m_txBlockSize == 0) m_txBlockSize = 1;
        m_txState = TX_SENDING_CF;
        sendNextConsecutiveFrame();
    } else if (flowStatus == FC_WT) {
        // Wait — 重启 N_As 定时器
        m_nAsTimer->start(N_AS_MS);
    } else if (flowStatus == FC_OVFLW) {
        m_nAsTimer->stop();
        m_txState = TX_IDLE;
        emit error("CanTp: FC overflow (peer buffer full)");
    }
}

void CanTp::sendNextConsecutiveFrame()
{
    if (m_txState != TX_SENDING_CF) return;

    for (uint8_t i = 0; i < m_txBlockSize && m_txBytesSent < (uint32_t)m_txBuffer.size(); ++i) {
        QByteArray cf;
        cf.append((char)(PCI_CF | (m_txSeqNum & 0x0F)));
        int remaining = m_txBuffer.size() - m_txBytesSent;
        int cfPayload = qMin(7, remaining);
        cf.append(m_txBuffer.mid(m_txBytesSent, cfPayload));
        while (cf.size() < 8) cf.append('\0');

        m_txBytesSent += cfPayload;
        m_txSeqNum = (m_txSeqNum + 1) & 0x0F;
        if (m_txSeqNum == 0) m_txSeqNum = 1;

        emit sendFrameRequest(m_requestId, cf);
    }

    if (m_txBytesSent >= (uint32_t)m_txBuffer.size()) {
        // 发送完成
        m_txState = TX_IDLE;
    } else {
        // 等待下一个 FC 来继续发下一块
        m_txState = TX_WAIT_FC;
        m_nAsTimer->start(N_AS_MS);
    }
}

// ==================== 超时处理 ====================

void CanTp::onNAsTimeout()
{
    m_txState = TX_IDLE;
    emit error("CanTp: N_As timeout (no FC received)");
}

void CanTp::onNCrTimeout()
{
    m_rxState = RX_IDLE;
    m_rxBuffer.clear();
    emit error("CanTp: N_Cr timeout (CF not received)");
}
