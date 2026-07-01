/**
 * COM 层信号定义 — 从 tools/can_signals.json 自动生成
 * 所有信号名和参数与 DBC 一致。
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
    SID_A_BUZZER_MODE, // 0x080 Mode                           
    SID_A_LED_STATE, // 0x081 State                          
    SID_B_FAN_TARGET, // 0x100 Target_Speed                   
    SID_B_WINDOW_TARGET, // 0x101 Target_Pos                     
    SID_B_BUZZER_MODE, // 0x102 Mode                           
    SID_B_LED_STATE, // 0x103 State                          
    SID_FLAGS_ULTRASONIC, // 0x180 Flags                          
    SID_DISTANCE_CM, // 0x180 Distance_cm                    
    SID_FLAGS_GEAR, // 0x181 Flags                          
    SID_GEAR, // 0x181 Gear                           
    SID_FLAGS_FUEL, // 0x182 Flags                          
    SID_FUEL_PERCENT_X10, // 0x182 Fuel_Percent_x10               
    SID_FLAGS_RPM, // 0x183 Flags                          
    SID_RPM_PERCENT_X10, // 0x183 Rpm_Percent_x10                
    SID_FLAGS_TEMPERATURE, // 0x200 Flags                          
    SID_TEMPERATURE_X10, // 0x200 Temperature_x10                
    SID_FLAGS_HUMIDITY, // 0x201 Flags                          
    SID_HUMIDITY_X10, // 0x201 Humidity_x10                   
    SID_FLAGS_FAN_ACTUAL, // 0x202 Flags                          
    SID_FAN_ACTUAL_SPEED, // 0x202 Actual_Speed                   
    SID_FLAGS_WINDOW_ACTUAL, // 0x203 Flags                          
    SID_WINDOW_ACTUAL_POS, // 0x203 Actual_Pos                     

    SID_COUNT
} SignalId_t;

/* ==================== 信号描述符 ==================== */

typedef struct {
    SignalId_t  id;
    uint32_t    can_id;
    uint8_t     byte_offset;
    uint8_t     bit_length;
    uint8_t     is_signed;
    uint8_t     has_flags;
} SignalDesc_t;

static const SignalDesc_t g_signal_desc[SID_COUNT] = {
    { SID_A_BUZZER_MODE, 0x080, 0, 8, 0, 0 },
    { SID_A_LED_STATE, 0x081, 0, 8, 0, 0 },
    { SID_B_FAN_TARGET, 0x100, 0, 8, 0, 0 },
    { SID_B_WINDOW_TARGET, 0x101, 0, 8, 0, 0 },
    { SID_B_BUZZER_MODE, 0x102, 0, 8, 0, 0 },
    { SID_B_LED_STATE, 0x103, 0, 8, 0, 0 },
    { SID_FLAGS_ULTRASONIC, 0x180, 0, 8, 0, 1 },
    { SID_DISTANCE_CM, 0x180, 1, 16, 0, 0 },
    { SID_FLAGS_GEAR, 0x181, 0, 8, 0, 1 },
    { SID_GEAR, 0x181, 1, 8, 0, 0 },
    { SID_FLAGS_FUEL, 0x182, 0, 8, 0, 1 },
    { SID_FUEL_PERCENT_X10, 0x182, 1, 16, 0, 0 },
    { SID_FLAGS_RPM, 0x183, 0, 8, 0, 1 },
    { SID_RPM_PERCENT_X10, 0x183, 1, 16, 0, 0 },
    { SID_FLAGS_TEMPERATURE, 0x200, 0, 8, 0, 1 },
    { SID_TEMPERATURE_X10, 0x200, 1, 16, 1, 0 },
    { SID_FLAGS_HUMIDITY, 0x201, 0, 8, 0, 1 },
    { SID_HUMIDITY_X10, 0x201, 1, 16, 0, 0 },
    { SID_FLAGS_FAN_ACTUAL, 0x202, 0, 8, 0, 1 },
    { SID_FAN_ACTUAL_SPEED, 0x202, 1, 8, 0, 0 },
    { SID_FLAGS_WINDOW_ACTUAL, 0x203, 0, 8, 0, 1 },
    { SID_WINDOW_ACTUAL_POS, 0x203, 1, 8, 0, 0 },
};

/* COM 层接口 */
void Com_Init(void);
int  Com_SendSignal(SignalId_t id, uint32_t value);
int  Com_SendFlags(SignalId_t flags_id, uint8_t valid, uint8_t counter);
int  Com_Flush(void);
int  Com_SendRaw(uint32_t can_id, const uint8_t *data, uint8_t dlc);
uint16_t Com_GetTxCount(void);
void     Com_ReceiveFrame(uint32_t can_id, const uint8_t *data, uint8_t dlc);
uint32_t Com_ReceiveSignal(SignalId_t id);
#ifdef __cplusplus
}
#endif

#endif /* COM_SIGNAL_DEFS_H */
