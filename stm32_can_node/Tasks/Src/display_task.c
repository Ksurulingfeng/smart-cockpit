#include "sys.h"
#include "task_headfile.h"
#include "task_config.h"
#include "oled_i2c_drv.h"
#include "key_drv.h"
#include "rte.h"
#include "debug.h"
#include <stdio.h>

#define PAGE_COUNT 2
static uint8_t page = 0;

void vDisplayTask(void *pvParameters)
{
    OLED_Clear();
#ifdef NODE_A
    page = 1;
    OLED_ShowText(0, 0, "     节点A   ", OLED_SIZE_8x16, OLED_MODE_NORMAL);
#elif defined(NODE_B)
    OLED_ShowText(0, 0, "     节点B   ", OLED_SIZE_8x16, OLED_MODE_NORMAL);
#endif

    uint8_t key;
    char text[24];
    for (;;) {
        if (xQueueReceive(xKeyQueue, &key, 0) == pdTRUE) {
            if (key == KEY_NEXT) {
                page = (page + 1) % PAGE_COUNT;
                OLED_Clear_Area(0, X_WIDTH - 1, 2, 8);
            } else if (key == KEY_PREV) {
                page = (page + PAGE_COUNT - 1) % PAGE_COUNT;
                OLED_Clear_Area(0, X_WIDTH - 1, 2, 8);
            }
        }
        if (page == 0) {
            sprintf(text, "温度:%.1f℃ ", Rte_Read(SID_TEMPERATURE_X10) / 10.0f);
            OLED_ShowText(0, 2, text, OLED_SIZE_8x16, OLED_MODE_NORMAL);
            sprintf(text, "湿度:%.1f%%  ", Rte_Read(SID_HUMIDITY_X10) / 10.0f);
            OLED_ShowText(0, 4, text, OLED_SIZE_8x16, OLED_MODE_NORMAL);
        } else if (page == 1) {
            sprintf(text, "转速:%.1f%%    ", Rte_Read(SID_RPM_PERCENT_X10) / 10.0f);
            OLED_ShowText(0, 2, text, OLED_SIZE_8x16, OLED_MODE_NORMAL);
            sprintf(text, "油量:%.1f%%    ", Rte_Read(SID_FUEL_PERCENT_X10) / 10.0f);
            OLED_ShowText(0, 4, text, OLED_SIZE_8x16, OLED_MODE_NORMAL);
            sprintf(text, "距离:%.1fcm ", (float)Rte_Read(SID_DISTANCE_CM));
            OLED_ShowText(0, 6, text, OLED_SIZE_8x16, OLED_MODE_NORMAL);
        }

        vTaskDelay(pdMS_TO_TICKS(TASK_CYCLE_DISPLAY));
    }
}