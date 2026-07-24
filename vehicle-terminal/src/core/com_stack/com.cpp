/**
 * @file    com.cpp
 * @brief   Com 层实现 — E2E 校验 + 信号解包/打包 + 诊断帧
 */

#include "com.h"
#include <QDebug>
#include <QByteArray>

// ==================== 单例 ====================

Com *Com::instance()
{
    static Com inst;
    return &inst;
}

Com::Com(QObject *parent)
    : QObject(parent)
{
}

// ==================== E2E 校验 ====================

bool Com::validateE2E(uint32_t canId, const uint8_t *data, uint8_t len,
                      const SignalDesc_t *flagsDesc)
{
    (void)len;

    if (!flagsDesc || !flagsDesc->has_flags)
        return true; // 控制帧，无 E2E

    uint8_t flags = data[0];
    bool valid    = CAN_GET_VALID(flags);
    if (!valid) {
        emit e2eError(canId, QString("valid flag is 0"));
        return false;
    }

    uint8_t counter = CAN_GET_COUNTER(flags);
    if (!m_counterInit[canId]) {
        m_counterInit[canId] = true;
        m_lastCounter[canId] = counter;
        return true;
    }

    uint8_t expected = (m_lastCounter[canId] + 1) & 0x0F;
    m_lastCounter[canId] = counter;

    if (counter != expected) {
        emit e2eError(canId, QString("counter jump: expected %1 got %2")
                              .arg(expected).arg(counter));
        return false;
    }

    return true;
}

// ==================== 信号解包 ====================

void Com::unpackAndEmit(uint32_t canId, const uint8_t *data)
{
    // 遍历信号描述符表，找出该 canId 下所有数据信号（非 flags 信号）
    for (int i = 0; i < SID_COUNT; ++i) {
        const SignalDesc_t *desc = &g_signal_desc[i];
        if (desc->can_id != canId || desc->has_flags)
            continue; // 跳过不相关的帧和 flags 信号

        uint32_t value = 0;
        int off         = desc->byte_offset;

        if (desc->bit_length == 8) {
            value = data[off];
        } else if (desc->bit_length == 16) {
            // 小端序解包
            value = (uint32_t)data[off] | ((uint32_t)data[off + 1] << 8);
        }

        // 有符号 16 位扩展
        if (desc->is_signed && desc->bit_length == 16) {
            value = (uint32_t)((int16_t)value);
        }

        // 缓存并发射
        m_signalCache[desc->id] = value;
        emit signalUpdated(desc->id, value);
    }
}

// ==================== 接收帧入口 ====================

void Com::receiveFrame(uint32_t canId, const uint8_t *data, uint8_t len)
{
    // ---- 诊断帧 (0x300) 特殊处理 ----
    if (canId == CAN_ID_ECU_DIAGREPORT) {
        if (len >= 8) {
            uint16_t tec      = (uint16_t)data[1] | ((uint16_t)data[2] << 8);
            uint16_t rec      = (uint16_t)data[3] | ((uint16_t)data[4] << 8);
            uint16_t txCount  = (uint16_t)data[5] | ((uint16_t)data[6] << 8);
            uint8_t  busLoad  = data[7];
            emit diagnosticDataReceived(tec, rec, txCount, busLoad);
        }
        return;
    }

    // ---- 传感器帧：查找 flags 信号做 E2E 校验 ----
    const SignalDesc_t *flagsDesc = nullptr;
    for (int i = 0; i < SID_COUNT; ++i) {
        if (g_signal_desc[i].can_id == canId && g_signal_desc[i].has_flags) {
            flagsDesc = &g_signal_desc[i];
            break;
        }
    }

    if (flagsDesc) {
        // 有 flags → 传感器帧 → E2E 校验
        validateE2E(canId, data, len, flagsDesc);
        // 即使校验失败也继续解包（宽松策略：能用则用）
        if (len < 2) return; // 传感器帧至少需要 flags + 1 字节数据
    }

    // ---- 解包数据信号 ----
    unpackAndEmit(canId, data);
}

// ==================== 信号发送 ====================

void Com::sendSignal(SignalId_t id, uint32_t value)
{
    if (id >= SID_COUNT) return;

    const SignalDesc_t *desc = &g_signal_desc[id];
    QByteArray data;

    // 有 flags 的帧：先填充 flags 字节（valid=1 + 递增计数器）
    // 注意：若信号本身就是 FLAGS 信号（byte_offset==0 && has_flags），则 value 就是 flags 值，
    // 只填 flags 不额外添加数据；否则 flags + 数据。
    bool isFlagsSignal = desc->has_flags && desc->byte_offset == 0;
    if (desc->has_flags) {
        uint8_t counter  = m_sendCounter[desc->can_id];
        m_sendCounter[desc->can_id] = (counter + 1) & 0x0F;
        uint8_t flags    = CAN_MAKE_FLAGS(1, counter);
        data.append((char)flags);
    }

    // 按位宽打包（小端序），FLAGS 信号本身不含额外 data
    if (!isFlagsSignal) {
        if (desc->bit_length == 8) {
            data.append((char)(uint8_t)value);
        } else if (desc->bit_length == 16) {
            data.append((char)(value & 0xFF));
            data.append((char)((value >> 8) & 0xFF));
        }
    }

    emit sendFrameRequest(desc->can_id, data);
}

// ==================== 管理接口 ====================

void Com::resetCounters()
{
    for (auto &v : m_counterInit) v = false;
    for (auto &v : m_sendCounter) v = 0;
}

uint32_t Com::getSignalValue(SignalId_t id) const
{
    if (id >= SID_COUNT) return 0;
    return m_signalCache[id];
}
