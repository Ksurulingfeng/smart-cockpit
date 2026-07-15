#include "sys.h"
#include "task_headfile.h"
#include "task_config.h"
#include "adc_drv.h"
#include "rte.h"
#include "debug.h"

#define FILTER_LEN 5
static uint16_t pot1_buf[FILTER_LEN];
static uint16_t pot2_buf[FILTER_LEN];
static uint8_t buf_index = 0;

static float filter_average(uint16_t *buf)
{
    uint32_t sum = 0;
    for (int i = 0; i < FILTER_LEN; i++) sum += buf[i];
    return sum / (float)FILTER_LEN;
}

void vADCTask(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    for (;;) {
        uint32_t pot1_raw = ADC_ReadChannel(&hadc1, ADC_CHANNEL_0);
        uint32_t pot2_raw = ADC_ReadChannel(&hadc1, ADC_CHANNEL_1);

        pot1_buf[buf_index] = pot1_raw;
        pot2_buf[buf_index] = pot2_raw;
        buf_index           = (buf_index + 1) % FILTER_LEN;

        float fuel = filter_average(pot1_buf) * 100.0f / 4095.0f;
        float rpm  = filter_average(pot2_buf) * 100.0f / 4095.0f;
        Rte_Write(SID_FUEL_PERCENT_X10, (uint32_t)(fuel * 10));
        Rte_Write(SID_RPM_PERCENT_X10, (uint32_t)(rpm * 10));

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(TASK_CYCLE_ADC));
    }
}
