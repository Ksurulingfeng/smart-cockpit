// Auto-generated from tools/can_signals.json
// 2026-07-24 14:47:08

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

#define CAN_ID_PCM_BUZZERCMD                     0x080
#define CAN_ID_PCM_LEDSTATECMD                   0x081

#define CAN_ID_BCM_FANTARGET                     0x100
#define CAN_ID_BCM_WINDOWTARGET                  0x101
#define CAN_ID_BCM_BUZZERCMD                     0x102
#define CAN_ID_BCM_LEDSTATECMD                   0x103

#define CAN_ID_PCM_PARKINGDISTANCE               0x180
#define CAN_ID_PCM_GEARPOSITION                  0x181
#define CAN_ID_PCM_FUELLEVEL                     0x182
#define CAN_ID_PCM_ENGINESPEED                   0x183

#define CAN_ID_BCM_CABINTEMPERATURE              0x200
#define CAN_ID_BCM_CABINHUMIDITY                 0x201
#define CAN_ID_BCM_FANACTUALSPEED                0x202
#define CAN_ID_BCM_WINDOWACTUALPOS               0x203

#define CAN_ID_ECU_DIAGREPORT                    0x300

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

typedef struct __attribute__((packed)) { uint8_t BuzzerMode; } PCM_BuzzerCmd_t;
typedef struct __attribute__((packed)) { uint8_t LED_State; } PCM_LEDStateCmd_t;
typedef struct __attribute__((packed)) { uint8_t FanTargetSpeed; } BCM_FanTarget_t;
typedef struct __attribute__((packed)) { uint8_t WindowTargetPos; } BCM_WindowTarget_t;
typedef struct __attribute__((packed)) { uint8_t BuzzerMode; } BCM_BuzzerCmd_t;
typedef struct __attribute__((packed)) { uint8_t LED_State; } BCM_LEDStateCmd_t;
typedef struct __attribute__((packed)) { uint8_t UltrasonicFlags; uint16_t ParkingDistance; } PCM_ParkingDistance_t;
typedef struct __attribute__((packed)) { uint8_t GearFlags; uint8_t GearPosition; } PCM_GearPosition_t;
typedef struct __attribute__((packed)) { uint8_t FuelFlags; uint16_t FuelLevel; } PCM_FuelLevel_t;
typedef struct __attribute__((packed)) { uint8_t RPM_Flags; uint16_t EngineSpeed; } PCM_EngineSpeed_t;
typedef struct __attribute__((packed)) { uint8_t TempFlags; int16_t CabinTemperature; } BCM_CabinTemperature_t;
typedef struct __attribute__((packed)) { uint8_t HumidFlags; uint16_t CabinHumidity; } BCM_CabinHumidity_t;
typedef struct __attribute__((packed)) { uint8_t FanFlags; uint8_t FanActualSpeed; } BCM_FanActualSpeed_t;
typedef struct __attribute__((packed)) { uint8_t WindowFlags; uint8_t WindowActualPos; } BCM_WindowActualPos_t;
typedef struct __attribute__((packed)) { uint8_t DiagHeader; uint16_t TEC_Counter; uint16_t REC_Counter; uint16_t TX_MessageCount; uint8_t BusLoadPercent; } ECU_DiagReport_t;

typedef struct { uint32_t id; uint8_t data[8]; uint8_t len; } CanRxMsg_t;

#ifdef __cplusplus
}
#endif

#endif
