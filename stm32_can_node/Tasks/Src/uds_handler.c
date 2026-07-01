// UDS 诊断服务 (ISO 14229-1)
#include "uds_handler.h"
#include "task_headfile.h"
#include "../../../shared/com_signal_defs.h"
#include "FreeRTOS.h"
#include "task.h"

#define UDS_SID_DIAG_CTRL   0x10
#define UDS_SID_READ_BY_ID  0x22
#define UDS_SID_TESTER_PRES 0x3E
#define UDS_SID_ECU_RESET   0x11
#define UDS_NRC_SERVICE_NOT_SUPPORTED 0x11
#define UDS_NRC_REQUEST_OUT_OF_RANGE  0x31
#define UDS_RESP_ID 0x7E8

static uint8_t  s_session    = 0x01;  // 当前诊断会话
static uint32_t s_s3_timeout = 0;     // S3 超时 tick 戳

static void send_pos(uint8_t sid, uint8_t *data, uint8_t len)
{
    uint8_t buf[8];
    buf[0] = sid | 0x40;
    for (uint8_t i = 0; i < len && i < 7; i++)
        buf[i + 1] = data[i];
    Com_SendRaw(UDS_RESP_ID, buf, len + 1);
}

static void send_neg(uint8_t sid, uint8_t nrc)
{
    uint8_t buf[3] = { 0x7F, sid, nrc };
    Com_SendRaw(UDS_RESP_ID, buf, 3);
}

void Uds_HandleRequest(uint8_t *data, uint8_t len)
{
    if (len < 1) return;
    uint8_t sid = data[0];

    switch (sid) {
    case UDS_SID_DIAG_CTRL: { // 0x10 诊断会话控制
        if (len < 2) { send_neg(sid, 0x13); break; }
        uint8_t session = data[1];
        if (session != 0x01 && session != 0x02 && session != 0x03) {
            send_neg(sid, 0x12); break;
        }
        s_session = session;
        if (session != 0x01)
            s_s3_timeout = xTaskGetTickCount() + pdMS_TO_TICKS(5000);
        uint8_t resp[4] = { session, 0, 50, 0 };
        send_pos(sid, resp, 4);
        break;
    }
    case UDS_SID_READ_BY_ID: { // 0x22 按ID读数据
        if (len < 3) { send_neg(sid, 0x13); break; }
        uint16_t did = ((uint16_t)data[1] << 8) | data[2];
        switch (did) {
        case 0xF1A0: { uint8_t v[] = { '1', '.', '0' }; send_pos(sid, v, 3); break; }
        case 0xF190: {
#ifdef NODE_A
            uint8_t v[] = "STM32-NODEA";
#elif defined(NODE_B)
            uint8_t v[] = "STM32-NODEB";
#endif
            send_pos(sid, v, 11); break;
        }
        case 0xF100: {
#ifdef NODE_A
            uint8_t v[] = "STM32-A";
#elif defined(NODE_B)
            uint8_t v[] = "STM32-B";
#endif
            send_pos(sid, v, 7); break;
        }
        case 0xF1B0: { uint8_t v = 0; send_pos(sid, &v, 1); break; }
        case 0xF1B1: {
            uint8_t v[2];
            v[0] = (uint8_t)(g_can_tec & 0xFF);
            v[1] = (uint8_t)((g_can_tec >> 8) & 0xFF);
            send_pos(sid, v, 2); break;
        }
        case 0xF1B2: {
            uint8_t v[2];
            v[0] = (uint8_t)(g_can_rec & 0xFF);
            v[1] = (uint8_t)((g_can_rec >> 8) & 0xFF);
            send_pos(sid, v, 2); break;
        }
        case 0xF1B3: {
            uint16_t cnt = Com_GetTxCount();
            uint8_t v[2];
            v[0] = (uint8_t)(cnt & 0xFF);
            v[1] = (uint8_t)((cnt >> 8) & 0xFF);
            send_pos(sid, v, 2); break;
        }
        default: send_neg(sid, UDS_NRC_REQUEST_OUT_OF_RANGE); break;
        }
        break;
    }
    case UDS_SID_TESTER_PRES: { // 0x3E 诊断会话保持
        if (len < 2) break;
        s_s3_timeout = xTaskGetTickCount() + pdMS_TO_TICKS(5000);
        if (data[1] != 0x00) {
            uint8_t sub = data[1];
            send_pos(sid, &sub, 1);
        }
        break;
    }
    case UDS_SID_ECU_RESET: { // 0x11 ECU复位
        if (len < 2) { send_neg(sid, 0x13); break; }
        uint8_t type = data[1];
        if (type != 0x01 && type != 0x03) {
            send_neg(sid, 0x12); break;
        }
        uint8_t ack = type;
        send_pos(sid, &ack, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        NVIC_SystemReset();
        break;
    }
    default:
        send_neg(sid, UDS_NRC_SERVICE_NOT_SUPPORTED);
        break;
    }
}

void Uds_CheckS3Timeout(void)
{
    if (s_session != 0x01 && xTaskGetTickCount() > s_s3_timeout)
        s_session = 0x01;
}
