// Auto-generated from tools/can_signals.json
// 2026-07-01 23:13:14

#ifndef COM_SIGNAL_DEFS_H
#define COM_SIGNAL_DEFS_H

#include "can_protocol_common.h"
#include "uds_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SID_A_BUZZER_MODE,
    SID_A_LED_STATE,
    SID_B_FAN_TARGET,
    SID_B_WINDOW_TARGET,
    SID_B_BUZZER_MODE,
    SID_B_LED_STATE,
    SID_FLAGS_ULTRASONIC,
    SID_DISTANCE_CM,
    SID_FLAGS_GEAR,
    SID_GEAR,
    SID_FLAGS_FUEL,
    SID_FUEL_PERCENT_X10,
    SID_FLAGS_RPM,
    SID_RPM_PERCENT_X10,
    SID_FLAGS_TEMPERATURE,
    SID_TEMPERATURE_X10,
    SID_FLAGS_HUMIDITY,
    SID_HUMIDITY_X10,
    SID_FLAGS_FAN_ACTUAL,
    SID_FAN_ACTUAL_SPEED,
    SID_FLAGS_WINDOW_ACTUAL,
    SID_WINDOW_ACTUAL_POS,
    SID_COUNT
} SignalId_t;

typedef enum {
    E2E_PROFILE_NONE    = 0,
    E2E_PROFILE_COUNTER = 1,
    E2E_PROFILE_CRC8    = 2,
} E2eProfile_t;

typedef struct {
    SignalId_t   id;
    uint32_t     can_id;
    uint8_t      byte_offset;
    uint8_t      bit_length;
    uint8_t      is_signed;
    uint8_t      has_flags;
    E2eProfile_t e2e_profile;
} SignalDesc_t;

static const SignalDesc_t g_signal_desc[SID_COUNT] = {
    { SID_A_BUZZER_MODE, CAN_ID_A_BUZZER_CTRL, 0, 8, 0, 0, E2E_PROFILE_NONE },
    { SID_A_LED_STATE, CAN_ID_A_LED_CTRL, 0, 8, 0, 0, E2E_PROFILE_NONE },
    { SID_B_FAN_TARGET, CAN_ID_B_FAN_TARGET_SPEED, 0, 8, 0, 0, E2E_PROFILE_NONE },
    { SID_B_WINDOW_TARGET, CAN_ID_B_WINDOW_TARGET_POS, 0, 8, 0, 0, E2E_PROFILE_NONE },
    { SID_B_BUZZER_MODE, CAN_ID_B_BUZZER_CTRL, 0, 8, 0, 0, E2E_PROFILE_NONE },
    { SID_B_LED_STATE, CAN_ID_B_LED_CTRL, 0, 8, 0, 0, E2E_PROFILE_NONE },
    { SID_FLAGS_ULTRASONIC, CAN_ID_A_ULTRASONIC, 0, 8, 0, 1, E2E_PROFILE_COUNTER },
    { SID_DISTANCE_CM, CAN_ID_A_ULTRASONIC, 1, 16, 0, 0, E2E_PROFILE_NONE },
    { SID_FLAGS_GEAR, CAN_ID_A_GEAR, 0, 8, 0, 1, E2E_PROFILE_COUNTER },
    { SID_GEAR, CAN_ID_A_GEAR, 1, 8, 0, 0, E2E_PROFILE_NONE },
    { SID_FLAGS_FUEL, CAN_ID_A_FUEL, 0, 8, 0, 1, E2E_PROFILE_COUNTER },
    { SID_FUEL_PERCENT_X10, CAN_ID_A_FUEL, 1, 16, 0, 0, E2E_PROFILE_NONE },
    { SID_FLAGS_RPM, CAN_ID_A_RPM, 0, 8, 0, 1, E2E_PROFILE_COUNTER },
    { SID_RPM_PERCENT_X10, CAN_ID_A_RPM, 1, 16, 0, 0, E2E_PROFILE_NONE },
    { SID_FLAGS_TEMPERATURE, CAN_ID_B_TEMPERATURE, 0, 8, 0, 1, E2E_PROFILE_COUNTER },
    { SID_TEMPERATURE_X10, CAN_ID_B_TEMPERATURE, 1, 16, 1, 0, E2E_PROFILE_NONE },
    { SID_FLAGS_HUMIDITY, CAN_ID_B_HUMIDITY, 0, 8, 0, 1, E2E_PROFILE_COUNTER },
    { SID_HUMIDITY_X10, CAN_ID_B_HUMIDITY, 1, 16, 0, 0, E2E_PROFILE_NONE },
    { SID_FLAGS_FAN_ACTUAL, CAN_ID_B_FAN_SPEED_ACTUAL, 0, 8, 0, 1, E2E_PROFILE_COUNTER },
    { SID_FAN_ACTUAL_SPEED, CAN_ID_B_FAN_SPEED_ACTUAL, 1, 8, 0, 0, E2E_PROFILE_NONE },
    { SID_FLAGS_WINDOW_ACTUAL, CAN_ID_B_WINDOW_POS_ACTUAL, 0, 8, 0, 1, E2E_PROFILE_COUNTER },
    { SID_WINDOW_ACTUAL_POS, CAN_ID_B_WINDOW_POS_ACTUAL, 1, 8, 0, 0, E2E_PROFILE_NONE },
};

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
