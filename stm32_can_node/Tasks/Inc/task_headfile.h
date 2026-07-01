#ifndef TASK_HEADFILE
#define TASK_HEADFILE

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

// 任务句柄
extern TaskHandle_t xActuatorTaskHandle;
extern TaskHandle_t xADCTaskHandle;
extern TaskHandle_t xCanTaskHandle;
extern TaskHandle_t xDisplayTaskHandle;
extern TaskHandle_t xHumitureTaskHandle;
extern TaskHandle_t xKeyTaskHandle;
extern TaskHandle_t xMonitorTaskHandle;
extern TaskHandle_t xUltrasonicTaskHandle;

// 任务函数
void vActuatorTask(void *pvParameters);
void vADCTask(void *pvParameters);
void vCanTask(void *pvParameters);
void vDisplayTask(void *pvParameters);
void vHumitureTask(void *pvParameters);
void vKeyTask(void *pvParameters);
void vMonitorTask(void *pvParameters);
void vUltrasonicTask(void *pvParameters);

// 队列句柄
extern QueueHandle_t xBuzzerQueue;
extern QueueHandle_t xCanRxQueue;
extern QueueHandle_t xFanQueue;
extern QueueHandle_t xKeyQueue;
extern QueueHandle_t xLEDQueue;
extern QueueHandle_t xWindowQueue;

// 全局变量
extern uint8_t g_gear;
extern float g_rpm_percent;
extern float g_fuel_percent;
extern float g_temperature;
extern float g_humidity;
extern float g_distance;

// 传感器有效标志
extern uint8_t g_ultrasonic_valid;
extern uint8_t g_temperature_valid;
extern uint8_t g_humidity_valid;

// 执行器实际值 (actuator_task 写入, CAN 任务读取上报)
extern uint8_t g_fan_actual_speed;
extern uint8_t g_window_actual_pos;

// CAN 错误状态 (ISR写入, 任务读取)
extern volatile uint8_t  g_can_error_state;
extern volatile uint16_t g_can_tec;
extern volatile uint16_t g_can_rec;

#endif /* TASK_HEADFILE */