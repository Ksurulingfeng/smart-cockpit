# COM 层信号系统设计详解

> 讲解本项目 COM 层的核心设计——信号描述符表、单工作缓冲区、代码生成管线。这是理解 AUTOSAR COM Stack 实际运作方式的最佳实践读物。

---

## 一、问题：为什么需要 COM 层

裸 CAN 编程的直接方式是把数据按字节手动填入 buffer 再发送：

```c
// 传统方式 —— 容易出错, 修改繁琐
uint8_t buf[8];
buf[0] = CAN_MAKE_FLAGS(valid, counter);  // 记住 byte0 是 flags
buf[1] = distance & 0xFF;                 // 记住 byte1-2 是距离
buf[2] = (distance >> 8) & 0xFF;
CAN_SendMessage(0x180, buf, 3);           // 记住 ID 是 0x180, DLC=3
```

问题：
1. **信号位置硬编码**——改协议布局要改所有收发代码
2. **CAN ID 分散**——同一帧的信号分散在多处代码
3. **两端不一致**——STM32 和 Qt 各自维护一份字节布局

COM 层的解决方案：**建立一个信号→CAN 帧的映射表（描述符表），收发代码通过信号名而不是字节偏移来操作。**

---

## 二、信号描述符表——整个 COM 层的核心

### 2.1 描述符结构

```c
// com_signal_defs.h
typedef enum {
    SID_DISTANCE_CM,         // 超声波距离 (0x180, byte1-2)
    SID_FLAGS_ULTRASONIC,    // 超声波有效标志 (0x180, byte0)
    SID_TEMPERATURE_X10,     // 温度值 (0x200, byte1-2)
    // ... 共 22 个信号
    SID_COUNT
} SignalId_t;

typedef struct {
    SignalId_t  id;            // 信号 ID
    uint32_t    can_id;        // 所属 CAN 帧 ID
    uint8_t     byte_offset;   // 在 8 字节 payload 中的起始字节
    uint8_t     bit_length;    // 信号位宽 (8/16)
    uint8_t     is_signed;     // 0=无符号, 1=有符号
    uint8_t     has_flags;     // 1=此帧 byte0 是 flags
} SignalDesc_t;
```

### 2.2 描述符表实例

```c
static const SignalDesc_t g_signal_desc[SID_COUNT] = {
    // 超声波帧 (0x180):  byte0=flags  byte1-2=distance
    { SID_FLAGS_ULTRASONIC,    CAN_ID_A_ULTRASONIC,  0, 8,  0, 1 },  // flags byte
    { SID_DISTANCE_CM,         CAN_ID_A_ULTRASONIC,  1, 16, 0, 0 },  // data  byte1-2

    // 温度帧 (0x200):     byte0=flags  byte1-2=temp (signed)
    { SID_FLAGS_TEMPERATURE,   CAN_ID_B_TEMPERATURE, 0, 8,  0, 1 },
    { SID_TEMPERATURE_X10,     CAN_ID_B_TEMPERATURE, 1, 16, 1, 0 },  // signed!

    // ... 其余 18 个信号
};
```

**关键洞察**：一张表同时服务于——
- **发送**：`SID_DISTANCE_CM` → `can_id=0x180`, `byte_offset=1`, `bit_length=16`
- **接收**：同样用 `can_id` 匹配当前帧，`byte_offset` 定位字节
- **代码生成**：由 `tools/dbc_generate_header.py` 从 JSON 自动生成

---

## 三、发送侧：单工作缓冲区设计

### 3.1 为什么只有一个 buffer

原始的"每个 CAN ID 一个 buffer"方案：

```c
static uint8_t g_frame_buf[0x400][8];  // 1024×8 = 8KB+
```

STM32F103C8Tx 只有 **20KB SRAM**。8KB buffer 占 40%，不可接受。

**优化方案**——单工作缓冲区：

```c
// com_layer.c
static uint8_t  g_tx_buf[8];     // 唯一缓冲区 = 8 bytes
static uint32_t g_tx_cid;        // 当前帧 CAN ID
static uint8_t  g_tx_dlc;        // 当前帧 DLC
static uint16_t g_tx_cnt;        // 累计发送帧数 (诊断帧用)
```

### 3.2 三步发送法

```c
// can_task.c: SWC 层的发送代码
Com_SendFlags(SID_FLAGS_ULTRASONIC, g_ultrasonic_valid, next_counter(&g_rc_ultrasonic));
Com_SendSignal(SID_DISTANCE_CM, (uint16_t)(g_distance > 0.0f ? g_distance : 0.0f));
Com_Flush();
```

`Com_SendFlags` 和 `Com_SendSignal` 实际上都调用同一个函数：

```c
int Com_SendSignal(SignalId_t id, uint32_t value)
{
    const SignalDesc_t *desc = &g_signal_desc[id];

    // 1. 按描述符写数据到缓冲区
    if (desc->bit_length == 8)
        g_tx_buf[desc->byte_offset] = (uint8_t)value;
    else if (desc->bit_length == 16) {
        g_tx_buf[desc->byte_offset]     = (uint8_t)(value & 0xFF);
        g_tx_buf[desc->byte_offset + 1] = (uint8_t)((value >> 8) & 0xFF);
    }

    // 2. 记录 CAN ID (从描述符表)
    g_tx_cid = desc->can_id;

    // 3. 更新 DLC = max(已有DLC, 当前信号末尾位置)
    uint8_t dlc = desc->byte_offset + (desc->bit_length / 8);
    if (desc->has_flags) dlc++;  // flags 占一个字节
    if (dlc > g_tx_dlc) g_tx_dlc = dlc;

    return 0;
}
```

然后 `Com_Flush()` 发送：

```c
int Com_Flush(void)
{
    if (g_tx_dlc == 0) return -1;

    g_tx_cnt++;  // 累计发送计数
    // 发送 (含 100 次重试 + taskYIELD)
    HAL_StatusTypeDef ret = CAN_SendMessage(g_tx_cid, g_tx_buf, g_tx_dlc);
    for (int retry = 0; retry < 100 && ret != HAL_OK; retry++) {
        delay_us(50);
        if ((retry + 1) % 10 == 0) taskYIELD();
        ret = CAN_SendMessage(g_tx_cid, g_tx_buf, g_tx_dlc);
    }

    // 重置缓冲区
    g_tx_buf[0] = 0;
    g_tx_dlc = 0;
    g_tx_cid = 0xFFFFFFFF;
    return (ret == HAL_OK) ? 0 : -1;
}
```

**设计要点**：
- CAN ID **完全不** 由 SWC 指定——描述符表保证正确
- DLC 自动计算——`byte_offset + signal_length + flags_byte`
- Flush 后才发送——多个信号可合并到一帧（AUTOSAR I-PDU 概念）
- 100 次重试 ~5ms——不永久阻塞
- `taskYIELD()` 每 10 次——防止最高优先级 CAN 任务饿死其他任务

### 3.3 为什么 Com_Flush 不需要参数

```c
// ✓ 正确
Com_SendSignal(SID_DISTANCE_CM, 150);
Com_Flush();  // g_tx_cid 已经在 Com_SendSignal 里被设为 0x180

// ✗ 原来的错误调用 —— 传 CAN ID 是多余的, 且可能传错
Com_Flush(CAN_ID_A_ULTRASONIC);
```

COM 层遵循 **"描述符表是唯一真理源"** 原则——CAN ID 只出现在描述符表里，不出现在 SWC 代码里。

---

## 四、接收侧：逆向解包

### 4.1 接收流程

```c
// can_task.c
static void process_rx_command(uint32_t id, uint8_t *data, uint8_t len)
{
    if (len < 1) return;                   // DLC 下限检查
    Com_ReceiveFrame(id, data, len);        // 1. 存帧上下文

    uint8_t val;
    switch (id) {
        case CAN_ID_B_FAN_TARGET_SPEED:
            val = (uint8_t)Com_ReceiveSignal(SID_B_FAN_TARGET);  // 2. 提取信号
            xQueueSend(xFanQueue, &val, pdMS_TO_TICKS(5));       // 3. 分发
            break;
        // ...
    }
}
```

### 4.2 接收实现

```c
void Com_ReceiveFrame(uint32_t can_id, const uint8_t *data, uint8_t dlc)
{
    g_rx_cid = can_id;
    g_rx_dlc = (dlc > 8) ? 8 : dlc;
    for (uint8_t i = 0; i < g_rx_dlc; i++)
        g_rx_buf[i] = data[i];             // 拷贝到接收缓冲
}

uint32_t Com_ReceiveSignal(SignalId_t id)
{
    const SignalDesc_t *desc = &g_signal_desc[id];
    if (desc->can_id != g_rx_cid) return 0;  // 帧不匹配→拒绝

    if (desc->bit_length == 16) {
        uint32_t val = g_rx_buf[desc->byte_offset]
                     | ((uint32_t)g_rx_buf[desc->byte_offset + 1] << 8);
        if (desc->is_signed)
            return (uint32_t)(int32_t)(int16_t)val;  // 符号扩展
        return val;
    }
    return g_rx_buf[desc->byte_offset];  // 8-bit
}
```

**安全特性**：`desc->can_id != g_rx_cid` 防御——如果错误调用 `Com_ReceiveSignal(SID_TEMPERATURE_X10)` 而当前帧是风扇帧 (0x202)，返回 0 而非垃圾数据。

---

## 五、代码生成管线

### 5.1 为什么用代码生成

手动维护描述符表 27 行 × 6 字段 = 162 个值，且需要与 DBC 文件、C 头文件三方一致。代码生成把单一 JSON 数据源自动转换为所有下游格式。

### 5.2 管线图

```
tools/can_signals.json (单一数据源, 15 messages × signal 字段)
    │
    ▼
tools/dbc_generate_header.py
    │
    ├─→ shared/can_protocol_common.h     (C 宏 + struct)
    └─→ shared/smart_cockpit.dbc         (工业标准 CAN 描述)
```

JSON 入口示例：

```json
{"id": 384, "name": "A_Ultrasonic", "dlc": 3, "sender": "NodeA",
 "signals": [
   {"name": "Flags",       "start": 0, "length": 8,  "signed": false, ...},
   {"name": "Distance_cm", "start": 8, "length": 16, "signed": false, ...}
]}
```

自动生成 C 代码：

```c
#define CAN_ID_A_ULTRASONIC  0x180
typedef struct __attribute__((packed)) {
    uint8_t  Flags;
    uint16_t Distance_cm;
} A_Ultrasonic_t;
```

自动生成 DBC：

```
BO_ 384 A_Ultrasonic: 3 NodeA
 SG_ Distance_cm : 8|16@1+ (1,0) [0|400] "cm" Master
```

### 5.3 编辑协议的正确流程

1. 编辑 `tools/can_signals.json`
2. 运行 `python tools/dbc_generate_header.py`
3. 重新编译两端——新信号即可用

---

## 六、与 AUTOSAR COM 的对照

| AUTOSAR 概念 | 本项目实现 |
|---|---|
| Signal | `SignalId_t` 枚举 |
| Signal Group | 一个 CAN ID 下的多个信号 |
| I-PDU | 单帧 8 字节 = `g_tx_buf[8]` |
| Com_SendSignal(SignalId, data) | `Com_SendSignal(SignalId_t, uint32_t)` |
| Com_ReceiveSignal(SignalId) | `Com_ReceiveSignal(SignalId_t)` |
| Com_TriggerIPDUSend | `Com_Flush()` |
| Signal Descriptor Table | `g_signal_desc[SID_COUNT]` |
| Code Generator | `tools/dbc_generate_header.py` |
| DBC Configuration | `shared/smart_cockpit.dbc` |

---

## 七、总结

1. **描述符表** 是整个 COM 层的核心——收发共享同一张表，单一数据源
2. **单工作缓冲区** 把 RAM 使用从 8KB 降到 15 bytes——嵌入式系统的关键考量
3. **Com_Flush() 无参** 不是缺陷——CAN ID 由描述符表保证，SWC 不应关心
4. **接收侧 Com_ReceiveSignal** 与发送侧 Com_SendSignal 对称——信号 ID → 描述符 → 字节操作
5. **代码生成** 从 JSON 同时产出 .h 和 .dbc——保证人工只维护一个文件

## 参考资料

- [com_signal_defs.h](../shared/com_signal_defs.h) — 描述符表 + API
- [com_layer.c](../stm32_can_node/Tasks/Src/com_layer.c) — STM32 端实现
- [tools/dbc_generate_header.py](../tools/dbc_generate_header.py) — 代码生成脚本
- [AUTOSAR SWS COM Specification](https://www.autosar.org/fileadmin/standards/R21-11/CP/AUTOSAR_SWS_COM.pdf)
