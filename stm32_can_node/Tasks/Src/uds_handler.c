// UDS 诊断服务 (ISO 14229-1)
#include "uds_handler.h"
#include "task_headfile.h"
#include "../../../shared/com_signal_defs.h"
#include "../../../shared/uds_protocol.h"
#include "FreeRTOS.h"
#include "task.h"

#define UDS_NRC_SUBFUNC_NOT_SUPPORTED 0x12
#define UDS_NRC_INCORRECT_LENGTH      0x13

static uint8_t  s_session  = 0x01;
static uint32_t s_s3_start = 0;

static void send_pos(uint8_t sid, const uint8_t *data, uint8_t len)
{
    uint8_t buf[8];
    buf[0] = sid | 0x40;
    if (len > 7) len = 7;
    for (uint8_t i = 0; i < len; i++)
        buf[i + 1] = data[i];
    Com_SendRaw(UDS_RESP_ID, buf, len + 1);
}

static void send_neg(uint8_t sid, uint8_t nrc)
{
    uint8_t buf[3] = { 0x7F, sid, nrc };
    Com_SendRaw(UDS_RESP_ID, buf, 3);
}

static void send_did_resp(uint8_t sid, uint16_t did, const uint8_t *data, uint8_t len)
{
    // ISO 14229-1: [SID+0x40] [DID_H] [DID_L] [data...]
    // SID(1) + DID(2) + data(len) <= 8
    if (len > 5) len = 5;
    uint8_t buf[8];
    buf[0] = sid | 0x40;
    buf[1] = (uint8_t)(did >> 8);
    buf[2] = (uint8_t)(did & 0xFF);
    for (uint8_t i = 0; i < len; i++)
        buf[3 + i] = data[i];
    Com_SendRaw(UDS_RESP_ID, buf, 3 + len);
}

void Uds_HandleRequest(uint8_t *data, uint8_t len)
{
    if (len < 1) return;
    uint8_t sid = data[0];

    if (s_session != 0x01)
        s_s3_start = xTaskGetTickCount();

    switch (sid) {
    case UDS_SID_DIAG_SESSION_CTRL: {
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
    case UDS_SID_READ_DATA_BY_ID: {
        if (len < 3) { send_neg(sid, UDS_NRC_INCORRECT_LENGTH); break; }
        uint16_t did = ((uint16_t)data[1] << 8) | data[2];
        switch (did) {
        case DID_SOFTWARE_VERSION:
            { uint8_t v[] = { '1', '.', '0' }; send_did_resp(sid, did, v, 3); break; }
        case DID_VIN_NUMBER:
#ifdef NODE_A
            { uint8_t v[] = "NODEA"; send_did_resp(sid, did, v, 5); break; }
#else
            { uint8_t v[] = "NODEB"; send_did_resp(sid, did, v, 5); break; }
#endif
        case DID_ECU_SERIAL:
#ifdef NODE_A
            { uint8_t v[] = "ST-A"; send_did_resp(sid, did, v, 4); break; }
#else
            { uint8_t v[] = "ST-B"; send_did_resp(sid, did, v, 4); break; }
#endif
        case DID_BUS_LOAD:
            { uint8_t v = 0; send_did_resp(sid, did, &v, 1); break; }
        case DID_TEC:
            { uint8_t v[2]; v[0] = (uint8_t)g_can_tec; v[1] = (uint8_t)(g_can_tec >> 8);
              send_did_resp(sid, did, v, 2); break; }
        case DID_REC:
            { uint8_t v[2]; v[0] = (uint8_t)g_can_rec; v[1] = (uint8_t)(g_can_rec >> 8);
              send_did_resp(sid, did, v, 2); break; }
        case DID_TX_COUNT:
            { uint16_t cnt = Com_GetTxCount();
              uint8_t v[2]; v[0] = (uint8_t)cnt; v[1] = (uint8_t)(cnt >> 8);
              send_did_resp(sid, did, v, 2); break; }
        default:
            send_neg(sid, UDS_NRC_REQUEST_OUT_OF_RANGE); break;
        }
        break;
    }
    case UDS_SID_TESTER_PRESENT: {
        if (len < 2) break;
        if ((data[1] & 0x80) == 0) {
            uint8_t sub = data[1];
            send_pos(sid, &sub, 1);
        }
        break;
    }
    case UDS_SID_ECU_RESET: {
        if (len < 2) { send_neg(sid, UDS_NRC_INCORRECT_LENGTH); break; }
        if (data[1] != 0x01 && data[1] != 0x03) {
            send_neg(sid, UDS_NRC_SUBFUNC_NOT_SUPPORTED); break;
        }
        send_pos(sid, &data[1], 1);
        vTaskDelay(pdMS_TO_TICKS(20));
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
