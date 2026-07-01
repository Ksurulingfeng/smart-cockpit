#include "sys.h"
#include "task_headfile.h"
#include "task_config.h"
#include "dht11_drv.h"
#include "debug.h"

float g_temperature = 0.0f;
float g_humidity    = 0.0f;

void vHumitureTask(void *pvParameters)
{
    for (;;) {
        vTaskSuspendAll();
        uint8_t ret = DHT11_Read_Data(&g_temperature, &g_humidity);
        xTaskResumeAll();

        if (ret == 0) {
            g_temperature_valid = 1;
            g_humidity_valid    = 1;
        } else {
            g_temperature_valid = 0;
            g_humidity_valid    = 0;
            debug_printf("[DHT11] Read failed, flags cleared\n");
        }
        vTaskDelay(pdMS_TO_TICKS(TASK_CYCLE_HUMITURE));
    }
}
