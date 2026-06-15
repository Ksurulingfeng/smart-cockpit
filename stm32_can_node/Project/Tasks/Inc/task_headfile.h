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

#endif /* TASK_HEADFILE */