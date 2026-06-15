#ifndef __CAN_PROTOCOL_H__
#define __CAN_PROTOCOL_H__

#include "sys.h"

/* ==================== PT-CAN (节点A - 动力域) ==================== */
// 节点A -> 主控 (上报)
#define CAN_ID_A_STATUS     0x100 // 心跳+按键
#define CAN_ID_A_ULTRASONIC 0x101 // 超声波距离 cm
#define CAN_ID_A_GEAR       0x102 // 档位
#define CAN_ID_A_FUEL       0x103 // 油量 0-100%
#define CAN_ID_A_RPM        0x104 // 转速 0-100%

// 主控 -> 节点A (控制)0
#define CAN_ID_A_BUZZER_CTRL 0x180 // 蜂鸣器模式
#define CAN_ID_A_LED_CTRL    0x181 // LED状态

/* ==================== Body-CAN (节点B - 车身域) ==================== */
// 节点B -> 主控 (上报)
#define CAN_ID_B_STATUS      0x200 // 心跳+按键
#define CAN_ID_B_TEMPERATURE 0x201 // 温度×10
#define CAN_ID_B_HUMIDITY    0x202 // 湿度×10

// 主控 -> 节点B (控制)
#define CAN_ID_B_FAN_TARGET_SPEED  0x280 // 风扇转速
#define CAN_ID_B_WINDOW_TARGET_POS 0x281 // 车窗目标位置
#define CAN_ID_B_BUZZER_CTRL       0x282 // 蜂鸣器模式
#define CAN_ID_B_LED_CTRL          0x283 // LED状态

/* ==================== 数据结构定义 ==================== */
typedef struct {
    uint32_t id;
    uint8_t data[8];
    uint8_t len;
} CanRxMsg_t;

typedef struct {
    uint8_t heartbeat;
} A_Status_t;

typedef struct {
    uint16_t distance_cm;
} A_Ultrasonic_t;

typedef struct {
    uint8_t gear; // D=1,P=2,R=3
} A_Gear_t;

typedef struct {
    uint16_t fuel_percent_x10; // 0-1000
} A_Fuel_t;

typedef struct {
    uint16_t rpm_percent_x10; // 0-1000
} A_Rpm_t;

typedef struct {
    uint8_t mode; // 0=off,1=single,2=continuous
} A_BuzzerCtrl_t;

typedef struct {
    uint8_t state; // 0=off,1=on
} A_LedCtrl_t;

typedef struct {
    uint8_t heartbeat;
} B_Status_t;

typedef struct {
    int16_t temperature_x10; // eg. 255 = 25.5°C
} B_Temperature_t;

typedef struct {
    uint16_t humidity_x10; // 0-1000
} B_Humidity_t;

typedef struct {
    uint8_t target_speed; // 0-100%
} B_FanTargetSpeed_t;

typedef struct {
    uint8_t target_pos; // 0-100%
} B_WindowTargetPos_t;

typedef struct {
    uint8_t mode;
} B_BuzzerCtrl_t;

typedef struct {
    uint8_t state;
} B_LedCtrl_t;

#endif /* __CAN_PROTOCOL_H */