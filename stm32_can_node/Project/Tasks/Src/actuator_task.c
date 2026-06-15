#include "sys.h"
#include "task_headfile.h"
#include "task_config.h"
#include "timers.h"
#include "buzzer_drv.h"
#include "led_drv.h"
#include "motor_drv.h"
#include "servo_drv.h"
#include "debug.h"

// 队列命令（对外协议）
typedef enum {
    CMD_BUZZER_OFF,
    CMD_BUZZER_ON,
    CMD_BUZZER_ON_200MS,
    CMD_BUZZER_ALERT,
} BuzzerCmd;

typedef enum {
    CMD_LED_OFF,
    CMD_LED_ON,
    CMD_LED_HEARTBEAT,
    CMD_LED_ALERT,
} LEDCmd;

// 定时器及状态
static TimerHandle_t buzzerTimer;
static TimerHandle_t ledTimer;
static uint8_t ledPhase;   // 心跳计数器 0-49
static uint8_t ledIsAlert; // 0=心跳, 1=警报

// 蜂鸣器：TimerID=NULL→200ms关断，非NULL→警报翻转
static void buzzerCb(TimerHandle_t xTimer)
{
    if (pvTimerGetTimerID(xTimer) != NULL) {
        static uint8_t on;
        on = !on;
        if (on)
            Buzzer_On();
        else
            Buzzer_Off();
        xTimerReset(xTimer, 0);
    } else {
        Buzzer_Off();
    }
}

// LED：100ms周期，心跳（双闪/5s）和警报（交替/200ms）共用
static void ledCb(TimerHandle_t xTimer)
{
    if (ledIsAlert) {
        if (ledPhase)
            LED_Off();
        else
            LED_On();
        ledPhase = !ledPhase;
    } else {
        if (ledPhase == 0 || ledPhase == 2)
            LED_On();
        else
            LED_Off();
        ledPhase = (ledPhase + 1) % 50;
    }
}

void vActuatorTask(void *pvParameters)
{
    uint8_t cmd = 0;

    buzzerTimer = xTimerCreate("Bz", pdMS_TO_TICKS(200), pdFALSE, (void *)0, buzzerCb);
    ledTimer    = xTimerCreate("Led", pdMS_TO_TICKS(100), pdTRUE, NULL, ledCb);

    for (;;) {
        // 蜂鸣器
        if (xQueueReceive(xBuzzerQueue, &cmd, 0) == pdTRUE) {
            xTimerStop(buzzerTimer, 0);
            switch (cmd) {
                case CMD_BUZZER_OFF:
                    Buzzer_Off();
                    break;
                case CMD_BUZZER_ON:
                    Buzzer_On();
                    break;
                case CMD_BUZZER_ON_200MS:
                    Buzzer_On();
                    vTimerSetTimerID(buzzerTimer, NULL);
                    xTimerChangePeriod(buzzerTimer, pdMS_TO_TICKS(200), 0);
                    xTimerStart(buzzerTimer, 0);
                    break;
                case CMD_BUZZER_ALERT:
                    Buzzer_On();
                    vTimerSetTimerID(buzzerTimer, (void *)1);
                    xTimerChangePeriod(buzzerTimer, pdMS_TO_TICKS(100), 0);
                    xTimerStart(buzzerTimer, 0);
                    break;
            }
        }

        // 风扇
        if (xQueueReceive(xFanQueue, &cmd, 0) == pdTRUE) {
            Motor_SetSpeed(cmd);
        }

        // 车窗
        if (xQueueReceive(xWindowQueue, &cmd, 0) == pdTRUE) {
            float angle = cmd / 100.0f * 180.0f;
            if (angle > 180.0f) angle = 180.0f;
            if (angle < 0.0f) angle = 0.0f;
            Servo_SetAngle(angle);
        }

        // LED
        if (xQueueReceive(xLEDQueue, &cmd, 0) == pdTRUE) {
            xTimerStop(ledTimer, 0);
            ledPhase = 0;
            switch (cmd) {
                case CMD_LED_OFF:
                    LED_Off();
                    break;
                case CMD_LED_ON:
                    LED_On();
                    break;
                case CMD_LED_HEARTBEAT:
                    ledIsAlert = 0;
                    xTimerStart(ledTimer, 0);
                    break;
                case CMD_LED_ALERT:
                    ledIsAlert = 1;
                    xTimerStart(ledTimer, 0);
                    break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(TASK_CYCLE_ACTUATOR));
    }
}