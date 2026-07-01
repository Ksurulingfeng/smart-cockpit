// Auto-generated from tools/can_signals.json
// 2026-06-30 16:23:44
//
// ID scheme: bit[10:8]=node  bit[7]=dir(0=ctrl)  bit[6:0]=signal
//   Master→A ctrl: 0x080-0x0FF  (highest priority)
//   Master→B ctrl: 0x100-0x17F
//   A→Master rpt:  0x180-0x1FF
//   B→Master rpt:  0x200-0x27F
//   Diagnostic:    0x300-0x37F  (lowest priority)

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

// ---- Master→A control (highest priority) (0x080-0x0FF) ----
#define CAN_ID_A_BUZZER_CTRL                     0x080
#define CAN_ID_A_LED_CTRL                        0x081

// ---- Master→B control (0x100-0x17F) ----
#define CAN_ID_B_FAN_TARGET_SPEED                0x100
#define CAN_ID_B_WINDOW_TARGET_POS               0x101
#define CAN_ID_B_BUZZER_CTRL                     0x102
#define CAN_ID_B_LED_CTRL                        0x103

// ---- A→Master report (0x180-0x1FF) ----
#define CAN_ID_A_ULTRASONIC                      0x180
#define CAN_ID_A_GEAR                            0x181
#define CAN_ID_A_FUEL                            0x182
#define CAN_ID_A_RPM                             0x183

// ---- B→Master report (0x200-0x27F) ----
#define CAN_ID_B_TEMPERATURE                     0x200
#define CAN_ID_B_HUMIDITY                        0x201
#define CAN_ID_B_FAN_SPEED_ACTUAL                0x202
#define CAN_ID_B_WINDOW_POS_ACTUAL               0x203

// ---- Diagnostic (lowest priority) (0x300-0x37F) ----
#define CAN_ID_DIAGNOSTIC                        0x300

// ---- Flags byte encoding ----
// byte0 bit[0]=valid  bit[7:4]=rolling_counter(0-15)  bit[3:1]=reserved
#define CAN_FLAG_VALID           0x01
#define CAN_ROLLING_COUNTER_MASK  0xF0
#define CAN_ROLLING_COUNTER_SHIFT 4
#define CAN_MAKE_FLAGS(v,c) ((((c)<<CAN_ROLLING_COUNTER_SHIFT)&CAN_ROLLING_COUNTER_MASK)|((v)?CAN_FLAG_VALID:0))
#define CAN_GET_VALID(f)    ((f)&CAN_FLAG_VALID)
#define CAN_GET_COUNTER(f)  (((f)&CAN_ROLLING_COUNTER_MASK)>>CAN_ROLLING_COUNTER_SHIFT)

// ---- Gear ----
#define CAN_GEAR_P 0x00 // fail-safe: open circuit → park
#define CAN_GEAR_R 0x01
#define CAN_GEAR_D 0x02

#define CAN_NODE_A_ID 0
#define CAN_NODE_B_ID 1

// ---- Actuator values ----
#define CAN_BUZZER_OFF   0x00
#define CAN_BUZZER_ALERT 0x03
#define CAN_LED_OFF      0x00
#define CAN_LED_ALERT    0x03

// ---- Frame structs (byte0=flags for sensor, byte0=payload for ctrl) ----
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
// A_Ultrasonic_t layout: 0:Flags  1:Distance_cm
// A_Gear_t layout: 0:Flags  1:Gear
// A_Fuel_t layout: 0:Flags  1:Fuel_Percent_x10
// A_Rpm_t layout: 0:Flags  1:Rpm_Percent_x10
// B_Temperature_t layout: 0:Flags  1:Temperature_x10
// B_Humidity_t layout: 0:Flags  1:Humidity_x10
// B_Fan_Speed_Actual_t layout: 0:Flags  1:Actual_Speed
// B_Window_Pos_Actual_t layout: 0:Flags  1:Actual_Pos
// Diagnostic_t layout: 0:Header  1:TEC  3:REC  5:TX_Msg_Count  7:Bus_Load

typedef struct { uint32_t id; uint8_t data[8]; uint8_t len; } CanRxMsg_t;

#ifdef __cplusplus
}
#endif

#endif
