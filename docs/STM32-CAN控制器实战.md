# STM32 CAN 控制器实战

> 以 STM32F103C8Tx 的 bxCAN 外设为案例，讲解 CAN 总线底层配置——位定时、滤波器、错误管理。

---

## 一、bxCAN 硬件能力

STM32F103 的 CAN 外设（bxCAN）支持 CAN 2.0A（11-bit 标准帧）和 CAN 2.0B（29-bit 扩展帧），本项目统一使用 11-bit 标准 ID。

| 资源 | 数量 | 说明 |
|---|---|---|
| 发送邮箱 | 3 | 硬件自动仲裁优先级 |
| 接收 FIFO | 2 个 × 3 帧深度 | 共 6 帧缓冲 |
| 滤波器组 | 14 个 | 可配置为 0-13 号组 |

---

## 二、位定时——采样点的核心

### 2.1 为什么采样点重要

CAN 总线没有独立时钟线。接收方从数据边沿提取时钟（位同步）。采样点决定了**在每一位的什么位置读取总线电平**——太早可能读到振铃，太晚可能在高速/长距下错过数据。

### 2.2 本项目配置

```c
// can.c: MX_CAN_Init()
hcan.Init.Prescaler    = 9;       // 36MHz / 9 = 4MHz CAN 内核时钟
hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
hcan.Init.TimeSeg1     = CAN_BS1_12TQ;
hcan.Init.TimeSeg2     = CAN_BS2_3TQ;
```

计算过程：

```
CAN 时钟 = APB1(36MHz) / Prescaler(9) = 4 MHz
TQ (时间量子) = 1 / 4MHz = 0.25μs

位时间 = SyncSeg(1TQ) + BS1(12TQ) + BS2(3TQ) = 16TQ = 4μs
波特率 = 1 / 4μs = 250 kbps  ← 工业车辆 CAN 典型速率

采样点 = (1 + 12) / 16 = 81.25%  ← 在 Bosch 推荐 75%-87.5% 范围内
```

### 2.3 为什么不用 CubeMX 默认值

CubeMX 默认：Prescaler=18, BS1=4TQ, BS2=3TQ → 采样点 = (1+4)/(1+4+3) = 62.5%

62.5% 太靠前——总线振铃未完全稳定时就读，在长距离或多节点场景易出错。推后到 81.25% 用 4 个额外 TQ 换取更大的噪声裕度。

### 2.4 SJW（同步跳转宽度）

```
SJW = 1TQ ≤ BS2(3TQ)  ✓
```

当检测到边沿与预期时间偏移时，最多调整 SJW 个 TQ 来重新同步。SJW 小 = 抗干扰强，SJW 大 = 同步恢复快。1TQ 是保守策略——宁可慢恢复，不错同步。

---

## 三、硬件滤波器

### 3.1 为什么需要

STM32F103 的 CAN 只有 6 帧 FIFO 深度。如果不设滤波器，**所有总线 ID 都会进入 FIFO**——相当于广播接收。密集总线（如 J1939 商用车的 50+ ID）下 6 帧深度瞬间溢出。

### 3.2 掩码模式工作原理

32-bit 掩码模式将 11-bit 标准 ID 左移 5 位放到高 16 位（CAN 2.0A 规范原因）：

```
FilterIdHigh:  ID << 5
MaskIdHigh:    MASK << 5

接收规则: (received_id << 5) & MaskIdHigh == FilterIdHigh & MaskIdHigh
```

### 3.3 本项目配置

**节点 A**（只收 0x080-0x081）：

```c
filter.FilterIdHigh     = (0x080 << 5);   // 期望 ID
filter.FilterMaskIdHigh = (0x7FE << 5);   // 掩码: bit0 不关心
// 0x7FE = 111 1111 1110
//          不关心→→→↑ (0x080 和 0x081 都通过)
```

```
接收 0x080 → (0x080 << 5) & (0x7FE << 5) == (0x080 << 5) & (0x7FE << 5) ✓
接收 0x081 → (0x081 << 5) & (0x7FE << 5) == (0x080 << 5) & (0x7FE << 5) ✓
接收 0x180 → (0x180 << 5) & (0x7FE << 5) ≠ (0x080 << 5) & (0x7FE << 5) ✗ 硬件丢弃
```

**节点 B**（只收 0x100-0x103）：

```c
filter.FilterIdHigh     = (0x100 << 5);
filter.FilterMaskIdHigh = (0x7FC << 5);   // bit[1:0] 不关心
// 0x7FC = 111 1111 1100 → 0x100/0x101/0x102/0x103 全通过
```

ISR 不再被无关帧唤醒，CPU 开销显著降低。

### 3.4 AutoRetransmission

```c
hcan.Init.AutoRetransmission = ENABLE;
```

当 CAN 帧因仲裁丢失或错误被 NAK 时，硬件自动重发——无需软件干预。配合下面的软件重试超时机制，形成双重保护：硬件处理瞬态冲突，软件处理持久故障。

---

## 四、发送超时——防死锁

### 4.1 问题分析

当 CAN 控制器进入 **Bus-Off 状态**，3 个发送邮箱全部不可用。此时 `HAL_CAN_AddTxMessage` 持续返回 `HAL_ERROR`：

```c
// 危险代码 —— CAN 任务优先级 4 (最高)
while (HAL_CAN_AddTxMessage(...) != HAL_OK) {
    delay_us(50);  // 不 yield! 其他 6 个任务全部饿死
}
```

结果：系统表现为"死机"——OLED 不再刷新、按键无响应。

### 4.2 本项目方案

```c
int Com_Flush(void)
{
    HAL_StatusTypeDef ret = CAN_SendMessage(g_tx_cid, g_tx_buf, g_tx_dlc);
    for (int retry = 0; retry < 100 && ret != HAL_OK; retry++) {
        delay_us(50);                         // 50μs 等待
        if ((retry + 1) % 10 == 0) taskYIELD();  // 每 500μs yield 一次
        ret = CAN_SendMessage(g_tx_cid, g_tx_buf, g_tx_dlc);
    }
    return (ret == HAL_OK) ? 0 : -1;         // 超时返回错误
}
// CAN 任务在下一个 20ms 周期自然重试
```

100 次 × 50μs = 5ms 总超时。每 10 次（500μs）yield 让出 CPU。

### 4.3 FreeRTOS 优先级分析

```
CAN 任务 (prio=4)  taskYIELD → 切换到 prio≤3 任务
  → ADC (prio=3), 超声波 (prio=3), 执行器 (prio=2), 按键 (prio=2),
     温湿度 (prio=2), OLED (prio=1)
  → 20ms 周期到了 → CAN 任务 (prio=4) 自动抢占回来继续重试
```

---

## 五、CAN 错误状态机

### 5.1 三种错误状态

| 状态 | TEC/REC 条件 | 行为 |
|---|---|---|
| **Error Active** | TEC<96 且 REC<96 | 正常通信，检测到错误时发 6 个显性位的 Active Error Flag |
| **Error Passive** | TEC≥96 或 REC≥96 | 仍可通信，但检测到错误时只发 6 个隐性位的 Passive Error Flag |
| **Bus-Off** | TEC>255 | 完全断开总线，不再发送或接收 |

TEC=发送错误计数，REC=接收错误计数。每检测到一个错误 +8，每成功一帧 -1。

### 5.2 本项目监控

```c
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
    if (hcan->Instance != CAN1) return;  // 仅处理 CAN1 实例
    g_can_tec = (uint16_t)((hcan->Instance->ESR >> 16) & 0xFF);  // ESR[23:16]
    g_can_rec = (uint16_t)((hcan->Instance->ESR >> 24) & 0xFF);  // ESR[31:24]

    // 先检查 BusOff 标志位——TEC 是 8-bit (max 255), 不会 >255
    if (hcan->Instance->ESR & CAN_ESR_BOFF)
        g_can_error_state = 3;       // BusOff
    else if (g_can_tec >= 128 || g_can_rec >= 128)
        g_can_error_state = 2;       // Error Passive
    else if (g_can_tec >= 96 || g_can_rec >= 96)
        g_can_error_state = 1;       // Warning
    else
        g_can_error_state = 0;       // OK
}
```

**BusOff 检测的关键**：不能检查 `g_can_tec > 255`——TEC 在 ESR 中只有 8-bit，最大 255，`>255` 永远为假。必须检查 `CAN_ESR_BOFF` 硬件标志位。

### 5.3 AutoBusOff 恢复

```c
hcan.Init.AutoBusOff = ENABLE;
```

BusOff 后硬件自动启动恢复流程——检测到 128 次 11 个连续隐性位后自动切回 Error Active。不启用则永久挂死。

---

## 六、接收 ISR 路径

```
CAN 总线边沿
  ↓
bxCAN 硬件接收+CRC 校验
  ↓
FIFO0 触发中断
  ↓ USB_LP_CAN1_RX0_IRQHandler
HAL_CAN_IRQHandler(&hcan)
  ↓ HAL_CAN_RxFifo0MsgPendingCallback
HAL_CAN_GetRxMessage(...)     ← 读 header + data
  ↓
rx_callback(...)              ← can_drv.c 注册的回调
  ↓ xQueueSendFromISR(...)
CanRxMsg_t 入 xCanRxQueue
  ↓ portYIELD_FROM_ISR
CAN 任务被唤醒，出队处理
```

ISR 路径上每个环节都在 5-10μs 内完成，不阻塞其他中断。

---

## 七、关键寄存器速查

| 寄存器 | 说明 | 使用场景 |
|---|---|---|
| ESR | Error Status Register | TEC [23:16], REC [31:24], BOFF flag |
| TSR | Transmit Status Register | 邮箱发送完成/失败状态 |
| RFxR | Receive FIFO x Register | FIFO 满/溢出/消息计数 |
| FMR | Filter Master Register | 滤波器初始化模式 |
| BTR | Bit Timing Register | 预分频+BS1+BS2（HAL 封装） |

---

## 八、总结

1. **采样点 81.25%** 通过 Prescaler=9+BS1=12+BS2=3 实现——推后提升噪声裕度
2. **硬件滤波器** 用掩码模式精确过滤——节点 A 只收 2 个 ID，节点 B 只收 4 个
3. **BusOff 检测** 必须检查 `CAN_ESR_BOFF` 位——TEC 是 8-bit 无法表达 >255
4. **发送超时** 5ms + taskYIELD——不阻塞其他任务
5. **AutoBusOff=ENABLE** 让硬件自动恢复，无需软件干预

## 参考资料

- STM32F103 Reference Manual (RM0008), Chapter 24: Controller area network (bxCAN)
- [can.c](../stm32_can_node/Core/Src/can.c) — HAL 初始化 + ISR 回调
- [can_drv.c](../stm32_can_node/Drivers/BSP/Src/can_drv.c) — BSP 封装
