#include "can_drv.h"

void (*rx_callback)(uint32_t id, uint8_t *data, uint8_t len) = NULL;

HAL_StatusTypeDef CAN_SendMessage(uint32_t id, uint8_t *data, uint8_t len)
{
    CAN_TxHeaderTypeDef tx_header;
    tx_header.StdId              = id;
    tx_header.IDE                = CAN_ID_STD;
    tx_header.RTR                = CAN_RTR_DATA;
    tx_header.DLC                = len;
    tx_header.TransmitGlobalTime = DISABLE;

    uint32_t tx_mailbox;
    return HAL_CAN_AddTxMessage(&hcan, &tx_header, data, &tx_mailbox);
}

void CAN_RegisterRxCallback(void (*callback)(uint32_t id, uint8_t *data, uint8_t len))
{
    rx_callback = callback;
}