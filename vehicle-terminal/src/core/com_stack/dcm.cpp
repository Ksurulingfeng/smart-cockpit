/**
 * @file    dcm.cpp
 * @brief   DCM 实现 — UDS 诊断服务 (ISO 14229-1)
 */

#include "dcm.h"
#include "com_stack/com.h"
#include "com_stack/dem.h"
#include <QDebug>

static constexpr int S3_SERVER_TIMEOUT_MS = 5000;

Dcm *Dcm::instance()
{
    static Dcm inst;
    return &inst;
}

Dcm::Dcm(QObject *parent)
    : QObject(parent)
{
    m_s3Timer = new QTimer(this);
    m_s3Timer->setSingleShot(true);
    connect(m_s3Timer, &QTimer::timeout, this, &Dcm::onSessionTimeout);
}

// ==================== 消息入口 ====================

void Dcm::processUdsMessage(const QByteArray &data)
{
    if (data.size() < 1) return;

    uint8_t sid = (uint8_t)data[0];

    switch (sid) {
    case UDS_SID_DIAG_SESSION_CTRL:  handleDiagSessionControl(data); break;
    case UDS_SID_ECU_RESET:             handleEcuReset(data); break;
    case UDS_SID_CLEAR_DTC:             handleClearDtcInfo(data); break;
    case UDS_SID_READ_DTC:              handleReadDtcInfo(data); break;
    case UDS_SID_READ_DATA_BY_ID:       handleReadDataByIdentifier(data); break;
    case UDS_SID_WRITE_DATA_BY_ID:      handleWriteDataByIdentifier(data); break;
    case UDS_SID_TESTER_PRESENT:        handleTesterPresent(data); break;
    default:
        sendNegativeResponse(sid, UDS_NRC_SERVICE_NOT_SUPPORTED);
        break;
    }
}

// ==================== 0x10 — 诊断会话控制 ====================

void Dcm::handleDiagSessionControl(const QByteArray &data)
{
    if (data.size() < 2) {
        sendNegativeResponse(UDS_SID_DIAG_SESSION_CTRL, UDS_NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    uint8_t session = (uint8_t)data[1];
    switch (session) {
    case UDS_SESSION_DEFAULT:
        m_s3Timer->stop();
        break;
    case UDS_SESSION_PROGRAMMING:
    case UDS_SESSION_EXTENDED:
        m_s3Timer->start(S3_SERVER_TIMEOUT_MS);
        break;
    default:
        sendNegativeResponse(UDS_SID_DIAG_SESSION_CTRL, UDS_NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    m_currentSession = session;
    QByteArray resp;
    resp.append((char)session);
    resp.append((char)0);   // P2_server 高字节
    resp.append((char)50);  // P2_server 低字节 (50ms)
    resp.append((char)0);
    resp.append((char)200); // P2_star (2000ms)
    sendPositiveResponse(UDS_SID_DIAG_SESSION_CTRL, resp);
    emit sessionChanged(session);
}

void Dcm::onSessionTimeout()
{
    m_currentSession = UDS_SESSION_DEFAULT;
    qDebug() << "[DCM] S3 timeout, fallback to default session";
    emit sessionChanged(UDS_SESSION_DEFAULT);
}

// ==================== 0x11 — ECU 复位 ====================

void Dcm::handleEcuReset(const QByteArray &data)
{
    if (data.size() < 2) {
        sendNegativeResponse(UDS_SID_ECU_RESET, UDS_NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    uint8_t resetType = (uint8_t)data[1];
    if (resetType != 0x01 && resetType != 0x03) {
        // 0x01 = 硬复位, 0x03 = 软复位
        sendNegativeResponse(UDS_SID_ECU_RESET, UDS_NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    // 先回复 ACK，再延时复位
    QByteArray resp;
    resp.append((char)resetType);
    sendPositiveResponse(UDS_SID_ECU_RESET, resp);

    QTimer::singleShot(200, []() {
        qDebug() << "[DCM] ECU reset requested, exiting...";
        exit(0);
    });
}

// ==================== 0x22 — 按 ID 读数据 ====================

void Dcm::handleReadDataByIdentifier(const QByteArray &data)
{
    if (data.size() < 3) {
        sendNegativeResponse(UDS_SID_READ_DATA_BY_ID, UDS_NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    uint16_t did = ((uint16_t)(uint8_t)data[1] << 8) | (uint8_t)data[2];
    QByteArray value;

    switch (did) {
    case DID_SOFTWARE_VERSION:
        value.append("1.0.0");
        break;
    case DID_VIN_NUMBER:
        value.append("SMARTCOCKPIT001");
        break;
    case DID_ECU_SERIAL:
        value.append("IMX6ULL-0001");
        break;
    case DID_BUS_LOAD:
        // 从 Com 缓存读取最近的总线负载（0x300 诊断帧更新）
        // 暂时返回 0，等待 Phase 6 集成
        value.append('\0');
        break;
    case DID_TEC:
    case DID_REC:
    case DID_TX_COUNT:
        // 诊断帧数据，暂返回 0 (后续从 Com 缓存读取)
        value.append((char)0).append((char)0);
        break;
    default:
        sendNegativeResponse(UDS_SID_READ_DATA_BY_ID, UDS_NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    QByteArray resp;
    resp.append((char)((did >> 8) & 0xFF));
    resp.append((char)(did & 0xFF));
    resp.append(value);
    sendPositiveResponse(UDS_SID_READ_DATA_BY_ID, resp);
}

// ==================== 0x2E — 按 ID 写数据 ====================

void Dcm::handleWriteDataByIdentifier(const QByteArray &data)
{
    if (data.size() < 4) {
        sendNegativeResponse(UDS_SID_WRITE_DATA_BY_ID, UDS_NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    uint16_t did = ((uint16_t)(uint8_t)data[1] << 8) | (uint8_t)data[2];

    // 仅在编程/扩展会话允许写入
    if (m_currentSession != UDS_SESSION_PROGRAMMING && m_currentSession != UDS_SESSION_EXTENDED) {
        sendNegativeResponse(UDS_SID_WRITE_DATA_BY_ID, UDS_NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    switch (did) {
    case DID_SOFTWARE_VERSION:
        sendNegativeResponse(UDS_SID_WRITE_DATA_BY_ID, UDS_NRC_REQUEST_OUT_OF_RANGE);
        return;
    default:
        sendNegativeResponse(UDS_SID_WRITE_DATA_BY_ID, UDS_NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    QByteArray resp;
    resp.append((char)((did >> 8) & 0xFF));
    resp.append((char)(did & 0xFF));
    sendPositiveResponse(UDS_SID_WRITE_DATA_BY_ID, resp);
}

// ==================== 0x3E — 诊断会话保持 ====================

void Dcm::handleTesterPresent(const QByteArray &data)
{
    (void)data;
    // 重置 S3 定时器
    if (m_currentSession != UDS_SESSION_DEFAULT)
        m_s3Timer->start(S3_SERVER_TIMEOUT_MS);

    // 零子功能 = 不需要回复
    if (data.size() >= 2 && (uint8_t)data[1] == 0x00)
        return;

    QByteArray resp;
    resp.append('\0'); // 零子功能回显
    sendPositiveResponse(UDS_SID_TESTER_PRESENT, resp);
}

// ==================== 0x19 — 读 DTC 信息 ====================

void Dcm::handleReadDtcInfo(const QByteArray &data)
{
    if (data.size() < 2) {
        sendNegativeResponse(UDS_SID_READ_DTC, UDS_NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    uint8_t subFunc = (uint8_t)data[1];

    switch (subFunc) {
    case 0x01: {
        // Report number of DTC by status mask
        uint8_t mask = (data.size() >= 3) ? (uint8_t)data[2] : 0xFF;
        uint8_t count = 0;
        for (const auto &e : Dem::instance()->dtcList()) {
            if (e.statusMask & mask) count++;
            if (count >= 254) break; // max
        }
        QByteArray resp;
        resp.append((char)mask);   // DTC 状态掩码
        resp.append((char)count);  // DTC 数量
        sendPositiveResponse(UDS_SID_READ_DTC, resp);
        break;
    }
    case 0x02: {
        // Report DTC by status mask
        uint8_t mask = (data.size() >= 3) ? (uint8_t)data[2] : 0xFF;
        QByteArray resp;
        resp.append((char)mask);
        for (const auto &e : Dem::instance()->dtcList()) {
            if (e.statusMask & mask) {
                resp.append((char)((e.dtc >> 16) & 0xFF));
                resp.append((char)((e.dtc >> 8) & 0xFF));
                resp.append((char)(e.dtc & 0xFF));
                resp.append((char)e.statusMask);
            }
        }
        sendPositiveResponse(UDS_SID_READ_DTC, resp);
        break;
    }
    default:
        sendNegativeResponse(UDS_SID_READ_DTC, UDS_NRC_REQUEST_OUT_OF_RANGE);
        break;
    }
}

// ==================== 0x14 — 清除 DTC ====================

void Dcm::handleClearDtcInfo(const QByteArray &data)
{
    (void)data;

    // 仅在默认/扩展会话允许清除
    if (m_currentSession != UDS_SESSION_DEFAULT && m_currentSession != UDS_SESSION_EXTENDED) {
        sendNegativeResponse(UDS_SID_CLEAR_DTC, UDS_NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    Dem::instance()->clearAllDtcs();
    sendPositiveResponse(UDS_SID_CLEAR_DTC, QByteArray());
}

// ==================== 响应打包 ====================

void Dcm::sendPositiveResponse(uint8_t sid, const QByteArray &data)
{
    QByteArray resp;
    resp.append((char)(sid | 0x40));  // positive response SID = SID + 0x40
    resp.append(data);
    emit sendUdsResponse(resp);
}

void Dcm::sendNegativeResponse(uint8_t sid, uint8_t nrc)
{
    QByteArray resp;
    resp.append((char)0x7F);   // Negative Response SID
    resp.append((char)sid);    // 原始 SID
    resp.append((char)nrc);    // NRC
    emit sendUdsResponse(resp);
}
