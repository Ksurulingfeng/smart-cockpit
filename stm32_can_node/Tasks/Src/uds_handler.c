// UDS 诊断服务 (ISO 14229-1)
#include "uds_handler.h"
#include "task_headfile.h"
#include "../../../shared/com_signal_defs.h"
#include "../../../shared/uds_protocol.h"
#include "FreeRTOS.h"
#include "task.h"

#define UDS_NRC_SUBFUNC_NOT_SUPPORTED 0x12
#define UDS_NRC_INCORRECT_LENGTH      0x13
#define UDS_RESP_ID 0x7E8

static uint8_t  s_session   = 0x01;
static uint32_t s_s3_start  = 0;       // S3 定时器起始 tick

static void send_pos(uint8_t sid, const uint8_t *data, uint8_t len)
{
    uint8_t buf[8];
    buf[0] = sid | 0x40;
    if (len > 7) len = 7;  // CAN 2.0 最大 8 字节 (1 SID + 7 data)
    for (uint8_t i = 0; i < len; i++)
        buf[i + 1] = data[i];
    Com_SendRaw(UDS_RESP_ID, buf, len + 1);
}

static void send_neg(uint8_t sid, uint8_t nrc)
{
    uint8_t buf[3] = { 0x7F, sid, nrc };
    Com_SendRaw(UDS_RESP_ID, buf, 3);
}

static void send_u16(uint8_t sid, uint16_t val)
{
    uint8_t v[2];
    v[0] = (uint8_t)(val & 0xFF);
    v[1] = (uint8_t)((val >> 8) & 0xFF);
    send_pos(sid, v, 2);
}

void Uds_HandleRequest(uint8_t *data, uint8_t len)
{
    if (len < 1) return;
    uint8_t sid = data[0];

    // 任何有效 UDS 请求均刷新 S3 定时器
    if (s_session != 0x01)
        s_s3_start = xTaskGetTickCount() + pdMS_TO_TICKS(5000);

    switch (sid) {
    case UDS_SID_DIAG_SESSION_CTRL: { // 0x10
        if (len < 2) { send_neg(sid, UDS_NRC_INCORRECT_LENGTH); break; }
        uint8_t session = data[1];
        if (session != 0x01 && session != 0x02 && session != 0x03) {
            send_neg(sid, UDS_NRC_SUBFUNC_NOT_SUPPORTED); break;
        }
        s_session = session;
        uint8_t resp[4] = { session, 0, 50, 200 };
        send_pos(sid, resp, 4);
        break;
    }
    case UDS_SID_READ_DATA_BY_ID: { // 0x22
        if (len < 3) { send_neg(sid, UDS_NRC_INCORRECT_LENGTH); break; }
        uint16_t did = ((uint16_t)data[1] << 8) | data[2];
        switch (did) {
        case DID_SOFTWARE_VERSION: { uint8_t v[] = { '1', '.', '0' }; send_pos(sid, v, 3); break; }
        case DID_VIN_NUMBER: {
#ifdef NODE_A
            uint8_t v[] = "STM32-NODEA";
#elif defined(NODE_B)
            uint8_t v[] = "STM32-NODEB";
#else
            uint8_t v[] = "STM32-NODE?";
#endif
            send_pos(sid, v, 11); break;
        }
        case DID_ECU_SERIAL: {
#ifdef NODE_A
            uint8_t v[] = "STM32-A";
#elif defined(NODE_B)
            uint8_t v[] = "STM32-B";
#else
            uint8_t v[] = "STM32-?";
#endif
            send_pos(sid, v, 7); break;
        }
        case DID_BUS_LOAD: { uint8_t v = 0; send_pos(sid, &v, 1); break; }
        case DID_TEC:  send_u16(sid, g_can_tec); break;
        case DID_REC:  send_u16(sid, g_can_rec); break;
        case DID_TX_COUNT: send_u16(sid, Com_GetTxCount()); break;
        default: send_neg(sid, UDS_NRC_REQUEST_OUT_OF_RANGE); break;
        }
        break;
    }
    case UDS_SID_TESTER_PRESENT: { // 0x3E
        if (len < 2) break;
        if ((data[1] & 0x80) == 0) {  // SPR=0 → 回复
            uint8_t sub = data[1];
            send_pos(sid, &sub, 1);
        }
        break;
    }
    case UDS_SID_ECU_RESET: { // 0x11
        if (len < 2) { send_neg(sid, UDS_NRC_INCORRECT_LENGTH); break; }
        uint8_t type = data[1];
        if (type != 0x01 && type != 0x03) {
            send_neg(sid, UDS_NRC_SUBFUNC_NOT_SUPPORTED); break;
        }
        uint8_t ack = type;
        send_pos(sid, &ack, 1);
        vTaskDelay(pdMS_TO_TICKS(20));   // 仅等 ACK 发出, 不阻塞太久
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
    if (s_session != 0x01
        && (xTaskGetTickCount() - s_s3_start) > pdMS_TO_TICKS(5000))
        s_session = 0x01;
}
