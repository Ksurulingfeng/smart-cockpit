#include "sys.h"
#include "task_headfile.h"
#include "task_config.h"
#include "can_drv.h"
#include "can_protocol.h"
#include "uds_handler.h"
#include "rte.h"
#include "debug.h"
#include <string.h>

volatile uint8_t g_can_error_state = 0;
volatile uint16_t g_can_tec        = 0;
volatile uint16_t g_can_rec        = 0;

// 接收信号计数器
static uint8_t g_rc_ultrasonic = 0;
static uint8_t g_rc_fuel       = 0;
static uint8_t g_rc_rpm        = 0;
static uint8_t g_rc_gear       = 0;
static uint8_t g_rc_temp       = 0;
static uint8_t g_rc_hum        = 0;
static uint8_t g_rc_fan        = 0;
static uint8_t g_rc_win        = 0;

// ISR回调: 收到CAN帧→入队xCanRxQueue
static void can_rx_callback(uint32_t id, uint8_t *data, uint8_t len)
{
    if (len > 8) len = 8;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    CanRxMsg_t msg;
    msg.id  = id;
    msg.len = len;
    memcpy(msg.data, data, len);
    xQueueSendFromISR(xCanRxQueue, &msg, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// 4-bit滚动计数器 +1 (0-15循环)
static uint8_t next_counter(uint8_t *c)
{
    *c = (*c + 1) & 0x0F;
    return *c;
}

// 诊断帧上报: TEC/REC/总线负载, 每10s一次
static void send_diag_report(uint8_t node_id)
{
    ECU_DiagReport_t diag;
    diag.DiagHeader       = (node_id << 4) | (g_can_error_state & 0x0F);
    diag.TEC_Counter          = g_can_tec;
    diag.REC_Counter          = g_can_rec;
    diag.TX_MessageCount = Com_GetTxCount();
    diag.BusLoadPercent     = 0;
    debug_printf("[DIAG] %s TEC=%d REC=%d State=%d TX=%d\r\n",
                 node_id ? "BCM" : "PCM", diag.TEC_Counter, diag.REC_Counter, g_can_error_state, diag.TX_MessageCount);
    Com_SendRaw(CAN_ID_ECU_DIAGREPORT, (uint8_t *)&diag, sizeof(diag));
}

#ifdef NODE_A

// PCM 数据帧上报: 超声波/档位/油量/转速 @100ms
static void send_node_a_data(void)
{
    static uint8_t count_data  = 0;
    static uint16_t count_diag = 0;

    if (++count_data * TASK_CYCLE_CAN >= 100) {
        count_data = 0;

        /* SWC: 超声波 */
        Com_SendFlags(SID_FLAGS_ULTRASONIC, Rte_IsValid(SID_FLAGS_ULTRASONIC), next_counter(&g_rc_ultrasonic));
        Com_SendSignal(SID_DISTANCE_CM, (uint16_t)Rte_Read(SID_DISTANCE_CM));
        Com_Flush();

        /* SWC: 档位 */
        Com_SendFlags(SID_FLAGS_GEAR, 1, next_counter(&g_rc_gear));
        Com_SendSignal(SID_GEAR, Rte_Read(SID_GEAR));
        Com_Flush();

        /* SWC: 油量 */
        Com_SendFlags(SID_FLAGS_FUEL, 1, next_counter(&g_rc_fuel));
        Com_SendSignal(SID_FUEL_PERCENT_X10, (uint16_t)Rte_Read(SID_FUEL_PERCENT_X10));
        Com_Flush();

        /* SWC: 转速 */
        Com_SendFlags(SID_FLAGS_RPM, 1, next_counter(&g_rc_rpm));
        Com_SendSignal(SID_RPM_PERCENT_X10, (uint16_t)Rte_Read(SID_RPM_PERCENT_X10));
        Com_Flush();
    }

    if (++count_diag * TASK_CYCLE_CAN >= 10000) {
        count_diag = 0;
        send_diag_report(CAN_NODE_A_ID);
    }
}

// 接收控制帧→COM解包→分发到执行器队列
static void process_rx_command(uint32_t id, uint8_t *data, uint8_t len)
{
    if (len < 1) return;
    Com_ReceiveFrame(id, data, len);
    uint8_t val;
    switch (id) {
        case CAN_ID_UDS_REQ_A:
            Uds_HandleRequest(data, len);
            break;
        case CAN_ID_PCM_BUZZERCMD:
            val = (uint8_t)Com_ReceiveSignal(SID_A_BUZZER_MODE);
            xQueueSend(xBuzzerQueue, &val, pdMS_TO_TICKS(5));
            break;
        case CAN_ID_PCM_LEDSTATECMD:
            val = (uint8_t)Com_ReceiveSignal(SID_A_LED_STATE);
            xQueueSend(xLEDQueue, &val, pdMS_TO_TICKS(5));
            break;
    }
}

#elif defined(NODE_B)

// BCM 数据帧上报: 温湿度+执行器反馈 @100ms
static void send_node_b_data(void)
{
    static uint8_t count_data  = 0;
    static uint16_t count_diag = 0;

    if (++count_data * TASK_CYCLE_CAN >= 100) {
        count_data = 0;

        /* SWC: 温度 */
        Com_SendFlags(SID_FLAGS_TEMPERATURE, Rte_IsValid(SID_FLAGS_TEMPERATURE), next_counter(&g_rc_temp));
        Com_SendSignal(SID_TEMPERATURE_X10, (int16_t)Rte_Read(SID_TEMPERATURE_X10));
        Com_Flush();

        /* SWC: 湿度 */
        Com_SendFlags(SID_FLAGS_HUMIDITY, Rte_IsValid(SID_FLAGS_HUMIDITY), next_counter(&g_rc_hum));
        Com_SendSignal(SID_HUMIDITY_X10, (uint16_t)Rte_Read(SID_HUMIDITY_X10));
        Com_Flush();

        /* SWC: 风扇实际反馈 */
        Com_SendFlags(SID_FLAGS_FAN_ACTUAL, 1, next_counter(&g_rc_fan));
        Com_SendSignal(SID_FAN_ACTUAL_SPEED, (uint8_t)Rte_Read(SID_FAN_ACTUAL_SPEED));
        Com_Flush();

        /* SWC: 车窗实际反馈 */
        Com_SendFlags(SID_FLAGS_WINDOW_ACTUAL, 1, next_counter(&g_rc_win));
        Com_SendSignal(SID_WINDOW_ACTUAL_POS, (uint8_t)Rte_Read(SID_WINDOW_ACTUAL_POS));
        Com_Flush();
    }

    if (++count_diag * TASK_CYCLE_CAN >= 10000) {
        count_diag = 0;
        send_diag_report(CAN_NODE_B_ID);
    }
}

// 接收控制帧→COM解包→分发到执行器队列
static void process_rx_command(uint32_t id, uint8_t *data, uint8_t len)
{
    if (len < 1) return;
    Com_ReceiveFrame(id, data, len);
    uint8_t val;
    switch (id) {
        case CAN_ID_UDS_REQ_B:
            Uds_HandleRequest(data, len);
            break;
        case CAN_ID_BCM_FANTARGET:
            val = (uint8_t)Com_ReceiveSignal(SID_B_FAN_TARGET);
            xQueueSend(xFanQueue, &val, pdMS_TO_TICKS(5));
            break;
        case CAN_ID_BCM_WINDOWTARGET:
            val = (uint8_t)Com_ReceiveSignal(SID_B_WINDOW_TARGET);
            xQueueSend(xWindowQueue, &val, pdMS_TO_TICKS(5));
            break;
        case CAN_ID_BCM_BUZZERCMD:
            val = (uint8_t)Com_ReceiveSignal(SID_B_BUZZER_MODE);
            xQueueSend(xBuzzerQueue, &val, pdMS_TO_TICKS(5));
            break;
        case CAN_ID_BCM_LEDSTATECMD:
            val = (uint8_t)Com_ReceiveSignal(SID_B_LED_STATE);
            xQueueSend(xLEDQueue, &val, pdMS_TO_TICKS(5));
            break;
    }
}

#endif

// CAN通信主任务: 周期性发送传感器数据+诊断, 处理接收队列
void vCanTask(void *pvParameters)
{
    CAN_RegisterRxCallback(can_rx_callback);

    TickType_t xLastWakeTime = xTaskGetTickCount();
    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(TASK_CYCLE_CAN));

#ifdef NODE_A
        send_node_a_data();
#elif defined(NODE_B)
        send_node_b_data();
#endif

        Uds_CheckS3Timeout();

        CanRxMsg_t rx_msg;
        while (xQueueReceive(xCanRxQueue, &rx_msg, 0) == pdTRUE) {
            process_rx_command(rx_msg.id, rx_msg.data, rx_msg.len);
        }
    }
}
