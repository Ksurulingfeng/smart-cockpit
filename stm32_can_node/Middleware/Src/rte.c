// RTE 层实现 — 信号缓冲 + 有效标志
#include "rte.h"
#include <string.h>

static uint32_t s_buf[SID_COUNT];
static uint8_t  s_valid[SID_COUNT];

void Rte_Init(void)
{
    memset(s_buf, 0, sizeof(s_buf));
    memset(s_valid, 0, sizeof(s_valid));
}

void Rte_Write(SignalId_t id, uint32_t value)
{
    if (id < SID_COUNT) s_buf[id] = value;
}

uint32_t Rte_Read(SignalId_t id)
{
    if (id < SID_COUNT) return s_buf[id];
    return 0;
}

void Rte_SetValid(SignalId_t flags_id, uint8_t valid)
{
    if (flags_id < SID_COUNT) s_valid[flags_id] = valid;
}

uint8_t Rte_IsValid(SignalId_t flags_id)
{
    if (flags_id < SID_COUNT) return s_valid[flags_id];
    return 0;
}
