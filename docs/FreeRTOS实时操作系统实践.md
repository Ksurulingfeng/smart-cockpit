# FreeRTOS 实时操作系统实践

> 以本项目 STM32 CAN 节点固件为案例，讲解 FreeRTOS 任务设计、优先级分配、队列与 ISR 通信的核心实践。

---

## 一、为什么需要 RTOS

裸机（Bare Metal）编程的核心问题是 **"一件事不干完不能干下一件"**：

```c
// 裸机 Super Loop
while (1) {
    读温湿度传感器();    // DHT11 要阻塞 20ms
    读超声波();          // HC-SR04 要等 38ms 回波
    更新OLED显示();      // I2C 传输 2ms
    检测按键();          // 要消抖，等 20ms
}
// 问题: 超声波等待期间按键完全无响应
```

RTOS 把每个功能拆成独立**任务（Task）**，由调度器自动切换——看起来像"同时运行"。

### FreeRTOS 核心概念

| 概念 | 说明 | 本项目中的对应 |
|---|---|---|
| **Task** | 独立执行单元，有独立栈 | `vCanTask`, `vADCTask` 等 7 个 |
| **Queue** | 任务间安全传递数据 | `xCanRxQueue`, `xBuzzerQueue` 等 6 个 |
| **ISR** | 硬件中断→通知任务 | CAN 接收 ISR, TIM3 捕获 ISR |
| **Scheduler** | 按优先级抢占式调度 | 高优先级就绪→立即抢占低优先级 |

---

## 二、本项目的任务架构

### 2.1 7 个任务全景图

```
                    优先级 4 (最高)
                   ┌──────────────────┐
                   │  vCanTask (20ms) │ ← CAN 收发 + 控制帧分发
                   └────────┬─────────┘
                            │
        优先级 3            │              优先级 3
   ┌──────────────┐        │        ┌──────────────────┐
   │ vADCTask     │        │        │ vUltrasonicTask  │
   │ (50ms)       │        │        │ (500ms)          │
   │ ADC 滤波     │        │        │ ISR 通知等待     │
   └──────────────┘        │        └──────────────────┘
                            │
        优先级 2            │              优先级 2
   ┌──────────────┐        │        ┌──────────────────┐
   │ vActuatorTask│        │        │ vKeyTask         │
   │ (20ms)       │        │        │ (20ms)           │
   │ 蜂鸣/LED/电机│        │        │ 按键消抖+档位    │
   └──────────────┘        │        └──────────────────┘
                            │        ┌──────────────────┐
                            │        │ vHumitureTask    │
                            │        │ (2s)             │
                            │        │ DHT11 单总线     │
                            │        └──────────────────┘
                            │
        优先级 1 (最低)
   ┌──────────────────────────────────┐
   │  vDisplayTask (200ms)            │
   │  OLED 2 页刷新 (I2C 模拟)        │
   └──────────────────────────────────┘
```

### 2.2 优先级为什么这样分

| 优先级 | 任务 | 分配理由 |
|---|---|---|
| **4 (最高)** | CAN 任务 | CAN 帧有实时性要求，ISR 入队后必须尽快出队消费，否则 FIFO 溢出丢帧 |
| **3** | ADC / 超声波 | 传感器采集有实时性要求但不丢帧，采样周期可容忍微调 |
| **2** | 执行器 / 按键 / 温湿度 | 20ms 执行器刷新可变通，2s 温湿度是慢速任务 |
| **1 (最低)** | OLED 显示 | 人眼刷新率要求低，200ms 延迟人眼察觉不到 |

**核心原则**：CAN 任务最高——STM32F103 邮箱仅 3 个 + FIFO 6 帧深度，不及时出队就会丢帧。

### 2.3 栈大小规划

```c
// task_config.h
#define TASK_STACK_CAN        192   // 调用 COM 层 + CAN HAL，栈消耗大
#define TASK_STACK_ACTUATOR   192   // 定时器回调嵌套
#define TASK_STACK_DISPLAY    192   // sprintf + 字符串操作
#define TASK_STACK_ADC        128   // 纯计算，无阻塞
#define TASK_STACK_HUMITURE   128   // DHT11 驱动
#define TASK_STACK_ULTRASONIC 128   // 简单状态机
#define TASK_STACK_KEY        64    // 只做位运算和队列发送
```

栈单位是**字（word）**，在 Cortex-M3 上 1 word = 4 bytes。所以 `TASK_STACK_CAN=192` 实际分配 768 bytes。20KB SRAM 中 7 个任务栈合计约 4KB，剩余 16KB 给全局变量、队列、HAL 开销。

---

## 三、三种任务间通信模式

### 模式 1：直接全局变量

**场景**：传感器采样→CAN 任务上报

```c
// adc_task.c: 生产者
float g_rpm_percent  = 0;
float g_fuel_percent = 0;

void vADCTask(void *pvParameters) {
    for (;;) {
        g_fuel_percent = filter_average(pot1_buf) * 100.0f / 4095.0f;
        g_rpm_percent  = filter_average(pot2_buf) * 100.0f / 4095.0f;
        vTaskDelayUntil(...);
    }
}

// can_task.c: 消费者 (任务级上下文, 无 ISR 竞争)
Com_SendSignal(SID_FUEL_PERCENT_X10, (uint16_t)(g_fuel_percent * 10));
```

**适用条件**：单一写入者 + 原子类型 + 非 ISR 上下文。这里全部满足——ADC 任务只写，CAN 任务只读，float 在 Cortex-M3 上为单字对齐读写。

### 模式 2：队列中转

**场景**：车辆终端 CAN 帧→执行器任务

```c
// ISR → 队列 → 任务
void can_rx_callback(uint32_t id, uint8_t *data, uint8_t len) {
    CanRxMsg_t msg;
    msg.id = id; msg.len = len;
    memcpy(msg.data, data, len);
    xQueueSendFromISR(xCanRxQueue, &msg, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);  // 抢占式: 立即调度
}

// CAN 任务出队
CanRxMsg_t rx_msg;
while (xQueueReceive(xCanRxQueue, &rx_msg, 0) == pdTRUE) {
    Com_ReceiveFrame(rx_msg.id, rx_msg.data, rx_msg.len);
    uint8_t val = (uint8_t)Com_ReceiveSignal(SID_xxx);
    xQueueSend(xFanQueue, &val, pdMS_TO_TICKS(5));
}
```

队列优点：自动处理数据复制（不共享指针）、阻塞/非阻塞选项、ISR 安全 API。

### 模式 3：任务通知（Task Notification）

**场景**：超声波回波捕获 ISR→超声波任务

```c
// ultrasonic_task.c
void vUltrasonicTask(void *pvParameters) {
    for (;;) {
        Ultrasonic_StartMeasure();                             // 发 Trig 脉冲
        uint32_t notif = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(500));
        // ↑ 阻塞等待, ISR 发通知时被唤醒
        if (notif > 0) {
            float dist = Ultrasonic_GetDistance();             // 读回波脉宽
            // 设置 g_distance + g_ultrasonic_valid
        } else {
            g_ultrasonic_valid = 0;  // 超时→传感器故障
        }
    }
}

// TIM3 输入捕获 ISR (stm32f1xx_it.c)
void TIM3_IRQHandler(void) {
    HAL_TIM_IRQHandler(&htim3);
    // 在 HAL_TIM_IC_CaptureCallback 中调用:
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(xUltrasonicTaskHandle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

任务通知比队列轻量（无内存分配）、比信号量传递更多信息（32-bit 值）。这里利用**超时机制**做传感器故障检测——500ms 内未收到回波说明传感器未响应。

### 三种模式对比

| 模式 | 适用场景 | 优势 | 劣势 |
|---|---|---|---|
| 全局变量 | 单一写入者+非 ISR | 零开销 | 无线程安全保证 |
| Queue | 多写者或 ISR→任务 | 线程安全+缓存 | 需分配队列内存 |
| Task Notify | 1对1 ISR→任务 | 比队列快 45% | 仅1对1 |

---

## 四、ISR 的设计策略

### 4.1 ISR 必须快

**错误做法**：ISR 里做复杂处理

```c
// ❌ ISR 里解析 CAN 帧 + 查表 + 写串口 —— 可能阻塞 100μs+
void CAN_ISR(void) {
    读FIFO → 解析payload → 更新UI数据结构 → printf调试输出
}
```

**正确做法**：ISR 只做最小工作

```c
// ✓ ISR 里只入队—— ≤5 条指令
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    CAN_RxHeaderTypeDef rx_header;
    uint8_t rx_data[8];
    HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data);
    if (rx_callback != NULL) rx_callback(rx_header.StdId, rx_data, rx_header.DLC);
    // 回调里只做 xQueueSendFromISR —— 不解析、不 log、不查表
}
```

### 4.2 中断优先级层次

FreeRTOS 刻意把 SysTick 和 PendSV 设为**最低优先级 (255)**，心跳绝不抢占硬件 ISR：

```
Cortex-M3 NVIC 优先级 (数值越小越优先, STM32F1 仅用高 4-bit)

  数值 12:    CAN RX0 IRQ   ← 实时数据, 不能丢帧, 实际最高
  数值 ~12:   TIM3 IRQ      ← 超声波捕获
  数值 ~12:   TIM4 IRQ      ← HAL 时基
  数值 15:    (其他外设)
  数值 255:   SysTick       ← FreeRTOS 心跳 (刻意最低!)
  数值 255:   PendSV        ← 上下文切换 (刻意最低!)
```

**为什么 SysTick 要最低**：如果 SysTick 优先级高于 CAN ISR，CAN ISR 执行期间可能被 SysTick 抢占导致任务切换——ISR 上下文中的任务切换会破坏硬件状态。FreeRTOS 的策略是：只有 PendSV（同样是 255）能触发切换，而 PendSV 只在所有高优先级 ISR 都结束后才执行。

**configMAX_SYSCALL_INTERRUPT_PRIORITY = 191**：NVIC 优先级 ≥191（即数值更大=优先级更低）的 ISR 可以安全调用 `xQueueSendFromISR` 等 FreeRTOS API。CAN RX0 的 HAL 优先级 12 在 8-bit 寄存器中实际为 `12 << 4 = 192 > 191`，恰好在安全范围内。

### 4.3 FreeRTOS API 的 ISR 版本

| 任务级 API | ISR 级 API | 区别 |
|---|---|---|
| `xQueueSend(q, p, timeout)` | `xQueueSendFromISR(q, p, &woken)` | ISR 不能阻塞→无 timeout |
| `vTaskNotifyGive(task)` | `vTaskNotifyGiveFromISR(task, &woken)` | ISR 触发任务通知 |
| — | `portYIELD_FROM_ISR(woken)` | 如果有更高优先级任务就绪，强制上下文切换 |

`pxHigherPriorityTaskWoken` 是 FreeRTOS ISR 的标准模式：ISR 入队后若唤醒更高优先级任务，此标志被置 `pdTRUE`，ISR 退出前调用 `portYIELD_FROM_ISR` 触发抢占。

---

## 五、关键实践

### 5.1 DHT11 临界区保护

DHT11 单总线协议对时序极度敏感（μs 级）。FreeRTOS 上下文切换会破坏时序：

```c
void vHumitureTask(void *pvParameters) {
    for (;;) {
        vTaskSuspendAll();   // ← 暂停调度器，禁止切换
        uint8_t ret = DHT11_Read_Data(&g_temperature, &g_humidity);
        xTaskResumeAll();    // ← 恢复调度
        // 临界区内 HAL_Delay 仍工作 (基于 TIM4 硬件定时器, 非 SysTick)
        ...
    }
}
```

`vTaskSuspendAll()` 不关中断（HAL_Delay 依赖 SysTick），但阻止任务切换——恰好满足 DHT11 对时序连续性的需求，又不破坏系统实时性。

### 5.2 滑动窗口均值滤波

ADC 读数有电气噪声，直接使用会导致显示跳变：

```c
#define FILTER_LEN 5
static uint16_t pot1_buf[FILTER_LEN];
static uint8_t buf_index = 0;

static float filter_average(uint16_t *buf) {
    uint32_t sum = 0;
    for (int i = 0; i < FILTER_LEN; i++) sum += buf[i];
    return sum / (float)FILTER_LEN;
}

void vADCTask(void *pvParameters) {
    for (;;) {
        pot1_buf[buf_index] = ADC_ReadChannel(&hadc1, ADC_CHANNEL_0);
        buf_index = (buf_index + 1) % FILTER_LEN;  // 环形缓冲
        g_fuel_percent = filter_average(pot1_buf) * 100.0f / 4095.0f;
        vTaskDelayUntil(...);
    }
}
```

5 点窗口 = 250ms 时域平滑。环形缓冲避免 memmove，O(1) 更新。

### 5.3 按键消抖状态机

不依赖 `vTaskDelay` 阻塞等待，而是用采样计数器实现非阻塞消抖：

```c
uint8_t key = Key_GetState();
if (key == key_last) {
    if (key_cnt < 3) key_cnt++;          // 连续 3 次 (60ms) 同值→确认
    if (key_cnt == 3 && key != KEY_NONE) {
        xQueueSend(xKeyQueue, &key, 0);  // 发送确认的按键
    }
} else {
    key_cnt = 0;                         // 值变了→重置计数
}
```

不阻塞任何任务——按键扫描只是 20ms 周期的一个循环迭代。

---

## 六、总结：FreeRTOS 任务设计清单

1. **CAN 任务优先级最高**——邮箱/FIFO 深度有限，不及时消费就丢帧
2. **ISR 只做入队**——不在 ISR 里解析、日志、查表
3. **用 `vTaskSuspendAll` 保护时间敏感外设**——DHT11 等短临界区无需关中断
4. **用 Task Notify 做 ISR→任务唤醒**——比队列更快，超时机制天然做故障检测
5. **常用 `vTaskDelayUntil` 而非 `vTaskDelay`**——保证绝对周期，不累积误差
6. **阻塞调用永远加超时**——`pdMS_TO_TICKS(500)` 不是 `portMAX_DELAY`，故障恢复

## 参考资料

- [FreeRTOS Kernel Developer Docs](https://www.freertos.org/features.html)
- [Mastering the FreeRTOS Real Time Kernel - A Hands On Tutorial Guide](https://www.freertos.org/Documentation/RTOS_book.html)
- 本项目源码：[init.c](../stm32_can_node/Core/Src/init.c)、[task_config.h](../stm32_can_node/Tasks/Inc/task_config.h)
