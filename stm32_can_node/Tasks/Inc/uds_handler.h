#ifndef UDS_HANDLER_H
#define UDS_HANDLER_H

#include "sys.h"

// 物理寻址: 每个节点独立 CAN ID 对，避免双节点响应冲突
#ifdef NODE_A
#define UDS_REQ_ID  0x7E0
#define UDS_RESP_ID 0x7E8
#else
#define UDS_REQ_ID  0x7E1
#define UDS_RESP_ID 0x7E9
#endif

void Uds_HandleRequest(uint8_t *data, uint8_t len);
void Uds_CheckS3Timeout(void);

#endif
