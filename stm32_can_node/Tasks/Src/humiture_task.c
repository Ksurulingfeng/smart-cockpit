#include "sys.h"
#include "task_headfile.h"
#include "task_config.h"
#include "dht11_drv.h"
#include "rte.h"
#include "debug.h"

void vHumitureTask(void *pvParameters)
{
    float temperature = 0.0f;
    float humidity    = 0.0f;

    for (;;) {
        vTaskSuspendAll();
        uint8_t ret = DHT11_Read_Data(&temperature, &humidity);
        xTaskResumeAll();

        if (ret == 0) {
            Rte_Write(SID_TEMPERATURE_X10, (uint32_t)(int32_t)(temperature * 10));
            Rte_Write(SID_HUMIDITY_X10, (uint32_t)(humidity * 10));
            Rte_SetValid(SID_FLAGS_TEMPERATURE, 1);
        } else {
            Rte_SetValid(SID_FLAGS_TEMPERATURE, 0);
            debug_printf("[DHT11] Read failed, flags cleared\n");
        }
        vTaskDelay(pdMS_TO_TICKS(TASK_CYCLE_HUMITURE));
    }
}
