// COM层: 信号↔CAN帧打包/解包
#include "sys.h"
#include "task_headfile.h"
#include "../../../shared/com_signal_defs.h"
#include "can_drv.h"
#include "debug.h"

// 发送侧: 单工作缓冲区
static uint8_t  g_tx_buf[8];
static uint32_t g_tx_cid;
static uint8_t  g_tx_dlc;
static uint16_t g_tx_cnt;

// 接收侧: 帧上下文
static uint8_t  g_rx_buf[8];
static uint32_t g_rx_cid;
static uint8_t  g_rx_dlc;

// 初始化COM层
void Com_Init(void)
{
    g_tx_cid = 0xFFFFFFFF;
    g_tx_dlc = 0;
    g_tx_cnt = 0;
}

// 写入信号值到帧缓冲区
int Com_SendSignal(SignalId_t id, uint32_t value)
{
    const SignalDesc_t *desc = &g_signal_desc[id];

    if (desc->bit_length == 8)
        g_tx_buf[desc->byte_offset] = (uint8_t)value;
    else if (desc->bit_length == 16) {
        g_tx_buf[desc->byte_offset]     = (uint8_t)(value & 0xFF);
        g_tx_buf[desc->byte_offset + 1] = (uint8_t)((value >> 8) & 0xFF);
    }

    g_tx_cid = desc->can_id;
    uint8_t dlc = desc->byte_offset + (desc->bit_length / 8);
    if (desc->has_flags) dlc++;
    if (dlc > g_tx_dlc) g_tx_dlc = dlc;
    return 0;
}

// 写入flags字节 (valid+counter)
int Com_SendFlags(SignalId_t flags_id, uint8_t valid, uint8_t counter)
{
    return Com_SendSignal(flags_id, CAN_MAKE_FLAGS(valid, counter));
}

// CAN 发送重试: 100次×50μs=5ms, 每10次 yield
static HAL_StatusTypeDef send_with_retry(uint32_t id, uint8_t *data, uint8_t dlc)
{
    HAL_StatusTypeDef ret = CAN_SendMessage(id, data, dlc);
    for (int retry = 0; retry < 100 && ret != HAL_OK; retry++) {
        delay_us(50);
        if ((retry + 1) % 10 == 0) taskYIELD();
        ret = CAN_SendMessage(id, data, dlc);
    }
    return ret;
}

// 发送当前帧并重置缓冲区
int Com_Flush(void)
{
    if (g_tx_dlc == 0) return -1;
    g_tx_cnt++;
    HAL_StatusTypeDef ret = send_with_retry(g_tx_cid, g_tx_buf, g_tx_dlc);
    g_tx_buf[0] = 0;
    g_tx_dlc = 0;
    g_tx_cid = 0xFFFFFFFF;
    return (ret == HAL_OK) ? 0 : -1;
}

uint16_t Com_GetTxCount(void) { return g_tx_cnt; }

// 直接发送原始数据 (诊断帧使用)
int Com_SendRaw(uint32_t can_id, const uint8_t *data, uint8_t dlc)
{
    g_tx_cnt++;
    return send_with_retry(can_id, (uint8_t *)data, dlc) == HAL_OK ? 0 : -1;
}

// 接收帧→存入接收上下文
void Com_ReceiveFrame(uint32_t can_id, const uint8_t *data, uint8_t dlc)
{
    g_rx_cid = can_id;
    g_rx_dlc = (dlc > 8) ? 8 : dlc;
    for (uint8_t i = 0; i < g_rx_dlc; i++)
        g_rx_buf[i] = data[i];
}

// 从接收帧按描述符提取信号 (含符号扩展)
uint32_t Com_ReceiveSignal(SignalId_t id)
{
    const SignalDesc_t *desc = &g_signal_desc[id];
    if (desc->can_id != g_rx_cid) return 0;

    if (desc->bit_length == 16) {
        uint32_t val = g_rx_buf[desc->byte_offset]
                     | ((uint32_t)g_rx_buf[desc->byte_offset + 1] << 8);
        if (desc->is_signed)
            return (uint32_t)(int32_t)(int16_t)val;
        return val;
    }
    return g_rx_buf[desc->byte_offset]; // 8-bit
}
