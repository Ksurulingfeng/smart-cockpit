#ifndef COMLAYER_H
#define COMLAYER_H

#include <cstdint>
#include "com_signal_defs.h"

class ComLayer
{
public:
    static ComLayer *instance();
    ComLayer(const ComLayer &) = delete;
    ComLayer &operator=(const ComLayer &) = delete;

    void processCanFrame(uint32_t canId, const uint8_t *data, uint8_t len);
    void sendSignal(SignalId_t id, uint32_t value);
    void resetCounters();

private:
    ComLayer() = default;
    uint8_t m_lastCounter[0x400] = {};
    bool    m_counterInit[0x400] = {};
};

#endif
