// Auto-generated from tools/can_signals.json
// 2026-07-01 21:45:21

#ifndef CAN_PROTOCOL_COMMON_H
#define CAN_PROTOCOL_COMMON_H

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define CAN_BITRATE_HZ 250000
#define CAN_HEARTBEAT_TIMEOUT_MS 3000

#define CAN_ID_A_BUZZER_CTRL                     0x080
#define CAN_ID_A_LED_CTRL                        0x081

#define CAN_ID_B_FAN_TARGET_SPEED                0x100
#define CAN_ID_B_WINDOW_TARGET_POS               0x101
#define CAN_ID_B_BUZZER_CTRL                     0x102
#define CAN_ID_B_LED_CTRL                        0x103

#define CAN_ID_A_ULTRASONIC                      0x180
#define CAN_ID_A_GEAR                            0x181
#define CAN_ID_A_FUEL                            0x182
#define CAN_ID_A_RPM                             0x183

#define CAN_ID_B_TEMPERATURE                     0x200
#define CAN_ID_B_HUMIDITY                        0x201
#define CAN_ID_B_FAN_SPEED_ACTUAL                0x202
#define CAN_ID_B_WINDOW_POS_ACTUAL               0x203

#define CAN_ID_DIAGNOSTIC                        0x300

#define CAN_FLAG_VALID           0x01
#define CAN_ROLLING_COUNTER_MASK  0xF0
#define CAN_ROLLING_COUNTER_SHIFT 4
#define CAN_MAKE_FLAGS(v,c) ((((c)<<CAN_ROLLING_COUNTER_SHIFT)&CAN_ROLLING_COUNTER_MASK)|((v)?CAN_FLAG_VALID:0))
#define CAN_GET_VALID(f)    ((f)&CAN_FLAG_VALID)
#define CAN_GET_COUNTER(f)  (((f)&CAN_ROLLING_COUNTER_MASK)>>CAN_ROLLING_COUNTER_SHIFT)

#define CAN_GEAR_P 0x00
#define CAN_GEAR_R 0x01
#define CAN_GEAR_D 0x02

#define CAN_NODE_A_ID 0
#define CAN_NODE_B_ID 1

#define CAN_BUZZER_OFF   0x00
#define CAN_BUZZER_ALERT 0x03
#define CAN_LED_OFF      0x00
#define CAN_LED_ALERT    0x03

typedef struct __attribute__((packed)) { uint8_t Mode; } A_Buzzer_Ctrl_t;
typedef struct __attribute__((packed)) { uint8_t State; } A_LED_Ctrl_t;
typedef struct __attribute__((packed)) { uint8_t Target_Speed; } B_Fan_Target_Speed_t;
typedef struct __attribute__((packed)) { uint8_t Target_Pos; } B_Window_Target_Pos_t;
typedef struct __attribute__((packed)) { uint8_t Mode; } B_Buzzer_Ctrl_t;
typedef struct __attribute__((packed)) { uint8_t State; } B_LED_Ctrl_t;
typedef struct __attribute__((packed)) { uint8_t Flags; uint16_t Distance_cm; } A_Ultrasonic_t;
typedef struct __attribute__((packed)) { uint8_t Flags; uint8_t Gear; } A_Gear_t;
typedef struct __attribute__((packed)) { uint8_t Flags; uint16_t Fuel_Percent_x10; } A_Fuel_t;
typedef struct __attribute__((packed)) { uint8_t Flags; uint16_t Rpm_Percent_x10; } A_Rpm_t;
typedef struct __attribute__((packed)) { uint8_t Flags; int16_t Temperature_x10; } B_Temperature_t;
typedef struct __attribute__((packed)) { uint8_t Flags; uint16_t Humidity_x10; } B_Humidity_t;
typedef struct __attribute__((packed)) { uint8_t Flags; uint8_t Actual_Speed; } B_Fan_Speed_Actual_t;
typedef struct __attribute__((packed)) { uint8_t Flags; uint8_t Actual_Pos; } B_Window_Pos_Actual_t;
typedef struct __attribute__((packed)) { uint8_t Header; uint16_t TEC; uint16_t REC; uint16_t TX_Msg_Count; uint8_t Bus_Load; } Diagnostic_t;

typedef struct { uint32_t id; uint8_t data[8]; uint8_t len; } CanRxMsg_t;

#ifdef __cplusplus
}
#endif

#endif
