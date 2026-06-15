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
            // debug_printf("温度：%.1f℃, 湿度：%.1f%%\r\n", g_temperature, g_humidity);
        } else {
            debug_printf("读取失败!\r\n");
        }
        vTaskDelay(pdMS_TO_TICKS(TASK_CYCLE_HUMITURE));
    }
}