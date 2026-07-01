#include "sys.h"
#include "task_headfile.h"
#include "task_config.h"
#include "key_drv.h"

uint8_t g_gear = GEAR_NONE;

void vKeyTask(void *pvParameters)
{
    uint8_t key_last = KEY_NONE;
    uint8_t key_sent = KEY_NONE;
    uint8_t key_cnt    = 0;

    uint8_t gear_last = GEAR_NONE;
    uint8_t gear_sent = GEAR_NONE;
    uint8_t gear_cnt     = 0;

    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        // 按键处理
        uint8_t key = Key_GetState();
        if (key == key_last) {
            if (key_cnt < 3) key_cnt++;
            if (key_cnt == 3 && key != KEY_NONE) {
                if (key != key_sent) {
                    xQueueSend(xKeyQueue, &key, 0);
                    key_sent = key;
                }
            }
        } else {
            key_cnt = 0;
        }
        key_last = key;
        if (key == KEY_NONE) {
            key_sent = KEY_NONE;
        }

        // 档位处理
        uint8_t gear = Gear_GetState();
        if (gear == gear_last) {
            if (gear_cnt < 3) gear_cnt++;
            if (gear_cnt == 3 && gear != GEAR_NONE) {
                if (gear != gear_sent) {
                    gear_sent = gear;
                    g_gear    = gear;
                }
            }
        } else {
            gear_cnt = 0;
        }
        gear_last = gear;
        if (gear == GEAR_NONE) {
            gear_sent = GEAR_NONE;
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(TASK_CYCLE_KEY));
    }
}