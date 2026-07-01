#ifndef __CAN_DRV_H__
#define __CAN_DRV_H__

#include "sys.h"

HAL_StatusTypeDef CAN_SendMessage(uint32_t id, uint8_t *data, uint8_t len);
void CAN_RegisterRxCallback(void (*callback)(uint32_t id, uint8_t *data, uint8_t len));
extern void (*rx_callback)(uint32_t id, uint8_t *data, uint8_t len);

#endif /* __CAN_DRV_H__ */