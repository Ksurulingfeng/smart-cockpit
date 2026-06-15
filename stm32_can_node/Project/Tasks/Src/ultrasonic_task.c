#include "sys.h"
#include "task_headfile.h"
#include "task_config.h"
#include "ultrasonic_drv.h"
#include "debug.h"

float g_distance = 0.0f;

void vUltrasonicTask(void *pvParameters)
{
    static float last_valid = 0.0f;

    for (;;) {
        Ultrasonic_StartMeasure();
        uint32_t ulNotificationValue = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(ULTRASONIC_TIMEOUT_MS));
        if (ulNotificationValue > 0) {
            float dist = Ultrasonic_GetDistance();
            if (dist >= 2.0f && dist <= ULTRANSONIC_MAX_DISTANCE_CM) {
                g_distance  = dist;
                last_valid  = dist;
            } else {
                g_distance = last_valid; // 偶发无效时保持上一次有效值
                debug_printf("Ultrasonic Invalid: %.2f cm\n", dist);
            }
        } else {
            g_distance = last_valid;
            debug_printf("Ultrasonic Timeout\n");
        }
        vTaskDelay(pdMS_TO_TICKS(TASK_CYCLE_ULTRASONIC));
    }
}