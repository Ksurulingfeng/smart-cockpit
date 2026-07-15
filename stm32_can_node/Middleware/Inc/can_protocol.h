/**
 * @file    can_protocol.h
 * @brief   STM32 CAN 节点协议入口
 *
 * 所有 CAN ID 和公共数据结构定义在 shared/can_protocol_common.h。
 * 本文件仅包含 STM32 平台依赖（sys.h）和固件专用补充类型。
 */

#ifndef __CAN_PROTOCOL_H__
#define __CAN_PROTOCOL_H__

#include "sys.h"
#include "../../../shared/can_protocol_common.h"
#include "../../../shared/com_signal_defs.h"

/* ==================== STM32 固件专用补充 ==================== */

/** CAN 接收回调函数类型（固件内部使用，非总线协议规范） */
typedef void (*CanRxCallback_t)(uint32_t id, uint8_t *data, uint8_t len);

#endif /* __CAN_PROTOCOL_H__ */
