#include "sys.h"
#include "task_headfile.h"
#include "task_config.h"
#include "led_drv.h"
#include "debug.h"

void vMonitorTask(void *pvParameters)
{
    for (;;) {
        // debug_printf("ActuatorTask water:\t%lu\n", uxTaskGetStackHighWaterMark(xActuatorTaskHandle));
        // debug_printf("ADCTask water:\t%lu\n", uxTaskGetStackHighWaterMark(xADCTaskHandle));
        // debug_printf("CanTask water:\t%lu\n", uxTaskGetStackHighWaterMark(xCanTaskHandle));
        // debug_printf("DisplayTask water:\t%lu\n", uxTaskGetStackHighWaterMark(xDisplayTaskHandle));
        // debug_printf("HumitureTask water:\t%lu\n", uxTaskGetStackHighWaterMark(xHumitureTaskHandle));
        // debug_printf("KeyTask water:\t%lu\n", uxTaskGetStackHighWaterMark(xKeyTaskHandle));
        // debug_printf("MonitorTask water:\t%lu\n", uxTaskGetStackHighWaterMark(NULL));
        // debug_printf("UltrasonicTask water:\t%lu\n", uxTaskGetStackHighWaterMark(xUltrasonicTaskHandle));

        vTaskDelay(pdMS_TO_TICKS(TASK_CYCLE_MONITOR));
    }
}
