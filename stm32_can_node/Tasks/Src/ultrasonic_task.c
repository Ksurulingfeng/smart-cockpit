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
        uint32_t notif = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(ULTRASONIC_TIMEOUT_MS));

        if (notif > 0) {
            float dist = Ultrasonic_GetDistance();
            if (dist >= 2.0f && dist <= ULTRANSONIC_MAX_DISTANCE_CM) {
                g_distance         = dist;
                last_valid         = dist;
                g_ultrasonic_valid = 1;
            } else {
                g_distance         = last_valid;
                g_ultrasonic_valid = 0;
            }
        } else {
            g_distance         = last_valid;
            g_ultrasonic_valid = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(TASK_CYCLE_ULTRASONIC));
    }
}
