// RTE 层 — 信号缓冲，传感器任务写入，CAN/显示任务读取
// 对标 AUTOSAR RTE: SWC 通过 Rte_Read/Write 交换信号，不碰全局变量
#ifndef RTE_H
#define RTE_H

#include "../../../shared/com_signal_defs.h"

void     Rte_Init(void);
void     Rte_Write(SignalId_t id, uint32_t value);
uint32_t Rte_Read(SignalId_t id);
void     Rte_SetValid(SignalId_t flags_id, uint8_t valid);
uint8_t  Rte_IsValid(SignalId_t flags_id);

#endif
