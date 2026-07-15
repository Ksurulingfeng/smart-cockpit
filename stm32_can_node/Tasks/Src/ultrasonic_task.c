#include "sys.h"
#include "task_headfile.h"
#include "task_config.h"
#include "ultrasonic_drv.h"
#include "rte.h"
#include "debug.h"

void vUltrasonicTask(void *pvParameters)
{
    static float last_valid = 0.0f;

    for (;;) {
        Ultrasonic_StartMeasure();
        uint32_t notif = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(ULTRASONIC_TIMEOUT_MS));

        if (notif > 0) {
            float dist = Ultrasonic_GetDistance();
            if (dist >= 2.0f && dist <= ULTRANSONIC_MAX_DISTANCE_CM) {
                last_valid = dist;
                Rte_Write(SID_DISTANCE_CM, (uint32_t)dist);
                Rte_SetValid(SID_FLAGS_ULTRASONIC, 1);
            } else {
                Rte_Write(SID_DISTANCE_CM, (uint32_t)last_valid);
                Rte_SetValid(SID_FLAGS_ULTRASONIC, 0);
            }
        } else {
            Rte_Write(SID_DISTANCE_CM, (uint32_t)last_valid);
            Rte_SetValid(SID_FLAGS_ULTRASONIC, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(TASK_CYCLE_ULTRASONIC));
    }
}
