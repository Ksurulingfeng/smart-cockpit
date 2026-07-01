/**
 * COM 层信号定义 — stm32_can_node 和 vehicle-terminal 共享
 *
 * 所有信号名和参数从 smart_cockpit.dbc 导出。
 * SWC 通过 Com_SendSignal(SID_xxx, value) 发送,
 *        通过 Com_ReceiveSignal(SID_xxx) 接收。
 * 不直接操作 CAN 帧。
 */

#ifndef COM_SIGNAL_DEFS_H
#define COM_SIGNAL_DEFS_H

#include "can_protocol_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 信号 ID 枚举 ==================== */

typedef enum {
    // 控制帧 — 主控→节点 (byte0=payload, 无flags)
    SID_A_BUZZER_MODE,              // 0x080, uint8
    SID_A_LED_STATE,                // 0x081, uint8
    SID_B_FAN_TARGET,               // 0x100, uint8
    SID_B_WINDOW_TARGET,            // 0x101, uint8
    SID_B_BUZZER_MODE,              // 0x102, uint8
    SID_B_LED_STATE,                // 0x103, uint8

    // 传感器帧 — flags 字段 (byte0=flags, 各信号共享)
    SID_FLAGS_ULTRASONIC,           // 0x180 byte0
    SID_FLAGS_GEAR,                 // 0x181 byte0
    SID_FLAGS_FUEL,                 // 0x182 byte0
    SID_FLAGS_RPM,                  // 0x183 byte0
    SID_FLAGS_TEMPERATURE,          // 0x200 byte0
    SID_FLAGS_HUMIDITY,             // 0x201 byte0
    SID_FLAGS_FAN_ACTUAL,           // 0x202 byte0
    SID_FLAGS_WINDOW_ACTUAL,        // 0x203 byte0

    // 传感器帧 — 数据字段
    SID_DISTANCE_CM,                // 0x180 byte1-2, uint16
    SID_GEAR,                       // 0x181 byte1,   uint8
    SID_FUEL_PERCENT_X10,           // 0x182 byte1-2, uint16
    SID_RPM_PERCENT_X10,            // 0x183 byte1-2, uint16
    SID_TEMPERATURE_X10,            // 0x200 byte1-2, int16
    SID_HUMIDITY_X10,               // 0x201 byte1-2, uint16
    SID_FAN_ACTUAL_SPEED,           // 0x202 byte1,   uint8
    SID_WINDOW_ACTUAL_POS,          // 0x203 byte1,   uint8

    SID_COUNT
} SignalId_t;

/* ==================== 信号描述符 ==================== */

typedef struct {
    SignalId_t  id;
    uint32_t    can_id;
    uint8_t     byte_offset;        // 在 8 字节 payload 的起始字节
    uint8_t     bit_length;         // 信号位宽 (8/16)
    uint8_t     is_signed;          // 0=unsigned, 1=signed
    uint8_t     has_flags;          // 1=此帧有 flags 字节 byte0
} SignalDesc_t;

/* 信号描述符表——由 COM 层内部使用, 编译期常量 */
static const SignalDesc_t g_signal_desc[SID_COUNT] = {
    { SID_A_BUZZER_MODE,       CAN_ID_A_BUZZER_CTRL,       0, 8, 0, 0 },
    { SID_A_LED_STATE,         CAN_ID_A_LED_CTRL,          0, 8, 0, 0 },
    { SID_B_FAN_TARGET,        CAN_ID_B_FAN_TARGET_SPEED,  0, 8, 0, 0 },
    { SID_B_WINDOW_TARGET,     CAN_ID_B_WINDOW_TARGET_POS, 0, 8, 0, 0 },
    { SID_B_BUZZER_MODE,       CAN_ID_B_BUZZER_CTRL,       0, 8, 0, 0 },
    { SID_B_LED_STATE,         CAN_ID_B_LED_CTRL,          0, 8, 0, 0 },

    { SID_FLAGS_ULTRASONIC,    CAN_ID_A_ULTRASONIC,        0, 8, 0, 1 },
    { SID_FLAGS_GEAR,          CAN_ID_A_GEAR,              0, 8, 0, 1 },
    { SID_FLAGS_FUEL,          CAN_ID_A_FUEL,              0, 8, 0, 1 },
    { SID_FLAGS_RPM,           CAN_ID_A_RPM,               0, 8, 0, 1 },
    { SID_FLAGS_TEMPERATURE,   CAN_ID_B_TEMPERATURE,       0, 8, 0, 1 },
    { SID_FLAGS_HUMIDITY,      CAN_ID_B_HUMIDITY,          0, 8, 0, 1 },
    { SID_FLAGS_FAN_ACTUAL,    CAN_ID_B_FAN_SPEED_ACTUAL,  0, 8, 0, 1 },
    { SID_FLAGS_WINDOW_ACTUAL, CAN_ID_B_WINDOW_POS_ACTUAL, 0, 8, 0, 1 },

    { SID_DISTANCE_CM,         CAN_ID_A_ULTRASONIC,        1, 16, 0, 0 },
    { SID_GEAR,                CAN_ID_A_GEAR,              1, 8,  0, 0 },
    { SID_FUEL_PERCENT_X10,    CAN_ID_A_FUEL,              1, 16, 0, 0 },
    { SID_RPM_PERCENT_X10,     CAN_ID_A_RPM,               1, 16, 0, 0 },
    { SID_TEMPERATURE_X10,     CAN_ID_B_TEMPERATURE,       1, 16, 1, 0 },
    { SID_HUMIDITY_X10,        CAN_ID_B_HUMIDITY,          1, 16, 0, 0 },
    { SID_FAN_ACTUAL_SPEED,    CAN_ID_B_FAN_SPEED_ACTUAL,  1, 8,  0, 0 },
    { SID_WINDOW_ACTUAL_POS,   CAN_ID_B_WINDOW_POS_ACTUAL, 1, 8,  0, 0 },
};

/* COM 层接口 */
void Com_Init(void);
// 发送
int  Com_SendSignal(SignalId_t id, uint32_t value);
int  Com_SendFlags(SignalId_t flags_id, uint8_t valid, uint8_t counter);
int  Com_Flush(void);
int  Com_SendRaw(uint32_t can_id, const uint8_t *data, uint8_t dlc);
uint16_t Com_GetTxCount(void);
// 接收
void     Com_ReceiveFrame(uint32_t can_id, const uint8_t *data, uint8_t dlc);
uint32_t Com_ReceiveSignal(SignalId_t id);
#ifdef __cplusplus
}
#endif

#endif /* COM_SIGNAL_DEFS_H */
