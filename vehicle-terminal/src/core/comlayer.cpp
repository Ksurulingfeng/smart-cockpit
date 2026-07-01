#include "comlayer.h"
#include "canmanager.h"
#include <QDebug>

ComLayer *ComLayer::instance()
{
    static ComLayer inst;
    return &inst;
}

void ComLayer::processCanFrame(uint32_t canId, const uint8_t *data, uint8_t len)
{
    (void)len;
    // 仅传感器上报帧有 flags 字节和 rolling counter
    bool isSensorFrame = (canId >= 0x180 && canId <= 0x183) || (canId >= 0x200 && canId <= 0x203);
    if (!isSensorFrame) return;

    uint8_t flags = data[0];
    if (!m_counterInit[canId]) {
        m_counterInit[canId] = true;
        m_lastCounter[canId] = CAN_GET_COUNTER(flags);
        return;
    }
    uint8_t counter = CAN_GET_COUNTER(flags);
    uint8_t expected = (m_lastCounter[canId] + 1) & 0x0F;
    m_lastCounter[canId] = counter;
    if (counter != expected)
        qDebug() << "[CAN] lost frame id=0x" << Qt::hex << canId
                 << "expected" << expected << "got" << counter;
}

void ComLayer::sendSignal(SignalId_t id, uint32_t value)
{
    const SignalDesc_t *desc = &g_signal_desc[id];
    QByteArray data;
    if (desc->bit_length == 8) {
        data.append((char)(uint8_t)value);
    } else if (desc->bit_length == 16) {
        data.append((char)(value & 0xFF));
        data.append((char)((value >> 8) & 0xFF));
    }
    CanManager::instance()->sendFrame(desc->can_id, data);
}

void ComLayer::resetCounters()
{
    for (auto &v : m_counterInit) v = false;
}
