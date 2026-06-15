#include "sys.h"
#include "task_headfile.h"
#include "task_config.h"
#include "can_drv.h"
#include "can_protocol.h"
#include "debug.h"
#include <string.h>

static uint8_t g_heartbeat_cnt = 0;

// 带重试的CAN发送，等待mailbox空闲
static void can_send_safe(uint32_t id, uint8_t *data, uint8_t len)
{
    while (CAN_SendMessage(id, data, len) != HAL_OK) {
        delay_us(50);
    }
}

// CAN接收回调
static void can_rx_callback(uint32_t id, uint8_t *data, uint8_t len)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    CanRxMsg_t msg;
    msg.id  = id;
    msg.len = len;
    memcpy(msg.data, data, len);
    xQueueSendFromISR(xCanRxQueue, &msg, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// 发送节点状态
static void send_node_status(void)
{
#ifdef NODE_A
    static uint8_t count_heartbeat_send = 0;
    if (++count_heartbeat_send * TASK_CYCLE_CAN >= 1000) // 每秒发送一次
    {
        count_heartbeat_send = 0;

        // 发送心跳
        A_Status_t status;
        status.heartbeat = g_heartbeat_cnt++;
        can_send_safe(CAN_ID_A_STATUS, (uint8_t *)&status, sizeof(status));

        debug_printf("[节点A(动力域)] 心跳:%d, 距离:%.1fcm, 档位:%d, 油量:%.1f%%, 转速:%.1f%%\n",
                     g_heartbeat_cnt, g_distance, g_gear, g_fuel_percent, g_rpm_percent);
    }

    static uint8_t count_data_send = 0;
    if (++count_data_send * TASK_CYCLE_CAN >= 100) // 每100ms发送一次
    {
        count_data_send = 0;

        // 发送超声波距离
        A_Ultrasonic_t us;
        us.distance_cm = (uint16_t)(g_distance > 0.0f ? g_distance : 0.0f);
        can_send_safe(CAN_ID_A_ULTRASONIC, (uint8_t *)&us, sizeof(us));

        // 发送档位
        A_Gear_t gear;
        gear.gear = g_gear;
        can_send_safe(CAN_ID_A_GEAR, (uint8_t *)&gear, sizeof(gear));

        // 发送油量
        A_Fuel_t fuel;
        fuel.fuel_percent_x10 = (uint16_t)(g_fuel_percent * 10);
        can_send_safe(CAN_ID_A_FUEL, (uint8_t *)&fuel, sizeof(fuel));

        // 发送转速
        A_Rpm_t rpm;
        rpm.rpm_percent_x10 = (uint16_t)(g_rpm_percent * 10);
        can_send_safe(CAN_ID_A_RPM, (uint8_t *)&rpm, sizeof(rpm));
    }
#elif defined(NODE_B)
    static uint8_t count_heartbeat_send = 0;
    if (++count_heartbeat_send * TASK_CYCLE_CAN >= 1000) // 每秒发送一次
    {
        count_heartbeat_send = 0;

        // 发送心跳
        B_Status_t status;
        status.heartbeat = g_heartbeat_cnt++;
        can_send_safe(CAN_ID_B_STATUS, (uint8_t *)&status, sizeof(status));

        debug_printf("[节点B(车身域)] 心跳:%d, 温度:%.1f℃, 湿度:%.1f%%\n",
                     g_heartbeat_cnt, g_temperature, g_humidity);
    }

    static uint8_t count_data_send = 0;
    if (++count_data_send * TASK_CYCLE_CAN >= 100) // 每100ms发送一次
    {
        count_data_send = 0;

        // 发送温度
        B_Temperature_t temp;
        temp.temperature_x10 = (int16_t)(g_temperature * 10);
        can_send_safe(CAN_ID_B_TEMPERATURE, (uint8_t *)&temp, sizeof(temp));

        // 发送湿度
        B_Humidity_t hum;
        hum.humidity_x10 = (uint16_t)(g_humidity * 10);
        can_send_safe(CAN_ID_B_HUMIDITY, (uint8_t *)&hum, sizeof(hum));
    }
#endif
}

// 处理接收到的CAN指令
static void process_rx_command(uint32_t id, uint8_t *data, uint8_t len)
{
    debug_printf("Received CAN ID: 0x%X, Data: ", id);
    for (uint8_t i = 0; i < len; i++) {
        debug_printf("%02X ", data[i]);
    }
    debug_printf("\n");
    switch (id) {
#ifdef NODE_A
        case CAN_ID_A_BUZZER_CTRL:
            xQueueSend(xBuzzerQueue, data, 0);
            break;
        case CAN_ID_A_LED_CTRL:
            xQueueSend(xLEDQueue, data, 0);
            break;
#elif defined(NODE_B)
        case CAN_ID_B_FAN_TARGET_SPEED:
            xQueueSend(xFanQueue, data, 0);
            break;
        case CAN_ID_B_WINDOW_TARGET_POS:
            xQueueSend(xWindowQueue, data, 0);
            break;
        case CAN_ID_B_BUZZER_CTRL:
            xQueueSend(xBuzzerQueue, data, 0);
            break;
        case CAN_ID_B_LED_CTRL:
            xQueueSend(xLEDQueue, data, 0);
            break;
#endif
    }
}

void vCanTask(void *pvParameters)
{
    CAN_RegisterRxCallback(can_rx_callback); // 注册CAN接收回调

    TickType_t xLastWakeTime = xTaskGetTickCount();
    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(TASK_CYCLE_CAN));
        send_node_status();

        /* 非阻塞处理接收队列中的CAN消息（节点B使用） */
        CanRxMsg_t rx_msg;
        while (xQueueReceive(xCanRxQueue, &rx_msg, 0) == pdTRUE) {
            process_rx_command(rx_msg.id, rx_msg.data, rx_msg.len);
        }
    }
}