# AUTOSAR 入门指南

> 面向不熟悉 AUTOSAR 的嵌入式开发者，结合本项目的 CAN 总线实践讲解。读完你会理解 AUTOSAR 的核心思想，以及你的项目"参考了 AUTOSAR E2E Profile 1"具体是什么意思。

---

## 一、AUTOSAR 是干什么的

### 1.1 汽车软件的"巴别塔"问题

假设你是宝马的工程师，要写一个"自动空调"功能：

```
需要读取: 温度传感器(博世) → CAN 总线
         阳光传感器(大陆) → LIN 总线
         车速(VDO)        → CAN 总线
需要控制: 鼓风机(电装)     → PWM
         压缩机(三电)     → GPIO
显示到:  中控屏(哈曼)     → 需调用对方的私有 API
```

你会发现——**每个供应商的接口都不一样，甚至同一个 CAN 总线上两个传感器的数据格式都不同**。如果你换一家供应商的温度传感器，整个空调控制逻辑都要重写。

**AUTOSAR 解决的就是这个问题**：定义一套标准化的软件架构，让不同供应商的 ECU 可以"即插即用"地集成到同一辆车上。

### 1.2 一句话总结

> AUTOSAR = 汽车嵌入式软件的**标准化操作系统抽象层 + 通信栈 + 方法论**。

它的核心思想是：**把应用逻辑和硬件彻底解耦**。你写"空调控制逻辑"时，不需要知道温度传感器是 SPI 还是 CAN 还是 LIN，只需要调用 `Rte_Read_Temperature()`。

---

## 二、两套平台：CP 和 AP

AUTOSAR 有两套标准，对应不同场景：

| 特性 | Classic Platform (CP) | Adaptive Platform (AP) |
|---|---|---|
| 目标 | 深度嵌入式 ECU | 高性能计算平台 |
| OS | OSEK/VDX 实时 OS | POSIX (Linux) |
| 语言 | C | C++ |
| 典型硬件 | 发动机 ECU、ABS | 自动驾驶域控、座舱 SoC |
| RAM | KB 级 | GB 级 |
| 你的项目对应 | **STM32 固件** | **Qt 车载终端** |

你的项目恰好覆盖了两端——STM32 端对标 CP（硬实时传感器采集+CAN 通信），Qt/Linux 端对标 AP（复杂应用逻辑+人机交互），是一个微型但完整的技术栈缩影。

---

## 三、Classic Platform 的软件分层

CP 把固件切成严格分层的栈，从上到下：

```
┌─────────────────────────────────────────────┐
│  Application Layer (应用层)                  │
│  ┌───────────┐ ┌───────────┐ ┌────────────┐ │
│  │ SWC_空调   │ │ SWC_车窗  │ │ SWC_发动机  │ │  ← 纯业务逻辑
│  └─────┬─────┘ └─────┬─────┘ └──────┬─────┘ │
├────────┼─────────────┼──────────────┼───────┤
│  RTE (Runtime Environment) ← 虚拟功能总线    │
├────────┼─────────────┼──────────────┼───────┤
│  BSW (Basic Software, 基础软件层)            │
│  ┌─────────────────────────────────────┐    │
│  │ Service Layer  (OS/内存/诊断)        │    │
│  │ ECU Abstraction (驱动抽象)           │    │
│  │ MCAL           (寄存器级驱动)         │    │
│  │ Complex Drivers (复杂驱动, 如CAN)    │    │
│  └─────────────────────────────────────┘    │
└─────────────────────────────────────────────┘
                 │
            ┌────┴────┐
            │  硬件    │
            └─────────┘
```

### 3.1 核心概念：SWC（软件组件）

SWC 是 AUTOSAR 的最小功能单元。一个 SWC 只包含**纯业务逻辑**，不直接操作任何硬件：

```
/// 一个典型的 SWC——只做逻辑，不碰硬件
void AirConditioner_SWC_Main(void) {
    int temp = Rte_Read_Temperature();      // 通过 RTE 读，不知道来源
    int target = Rte_Read_TargetTemp();     // 通过 RTE 读，不知道谁设的
    if (temp > target) {
        Rte_Call_CompressorOn();            // 通过 RTE 调，不知道压缩机在哪
    }
}
```

`Rte_Read_Temperature()` 实际上是在通过 CAN 总线从发动机 ECU 读温度传感器——但 SWC 不知道、也不需要知道。这是 AUTOSAR 最核心的设计哲学。

### 3.2 核心概念：RTE（运行时环境）

RTE 是 SWC 之间的"中间人"。它维护一个**虚拟功能总线（VFB）**——所有 SWC 看到的都是同样的 `Rte_Read_xxx()` / `Rte_Write_xxx()` 接口，RTE 在背后根据配置文件把调用路由到真实硬件或另一个 SWC。

**你的项目中的类比**：`Rte::instance()` 就是你的"RTE"。各个页面不直接读 CAN 总线——它们通过 `Rte` 的信号（`speedChanged`、`temperatureChanged` 等）获取数据。如果哪天你把温度来源从 CAN 换成串口，只需改 `Rte`，所有页面代码不动。`Rte::setMode(Rte::MOCK)` 可以一键切换到模拟数据源，正是 VFB（虚拟功能总线）的体现。

---

## 四、通信栈（ComStack）——和你的项目关系最密切的部分

AUTOSAR 的通信栈是分层设计，每一层只做一件事。**你的 CAN 实现可以映射到各层**：

```
AUTOSAR 层              你的实现 (已实现完整分层)
──────────              ────────
SWC (应用)              HomePage, rearViewCamera  ← 不碰 CAN, 用 Rte 信号
  ↓ Rte_Write()
RTE                     rte.h/rte.cpp             ← 模式切换 CAN/Mock, 虚拟功能总线
  ↓ Com_SendSignal()
COM                     com_signal_defs.h + com_layer.c (STM32)
                        comlayer.h/cpp (Qt)        ← 信号描述符表, 自动打包/解包
  ↓ PduR_Transmit()                               ← Com_SendSignal/Com_ReceiveSignal 双向
PDU Router              (只有 CAN, 直接调用)       ← 单总线系统无需路由
  ↓ CanIf_Transmit()
CAN Interface           can_drv.c / CanManager    ← HAL 抽象 + SocketCAN
  ↓ Can_Write()
CAN Controller          STM32F103 bxCAN           ← 硬件
```

**COM 层**是 AUTOSAR 最关键的一层——它把"信号"（比如 Temperature=25.5°C）打包成"PDU"（Protocol Data Unit，即 CAN 帧的 8 字节数据），并负责信号的字节序、缩放因子。发送路径 `Com_SendSignal(SID, value)` 通过信号描述符表自动定位目标帧和字节偏移，`Com_Flush()` 触发实际发送。接收路径 `Com_ReceiveFrame()` + `Com_ReceiveSignal(SID)` 逆向解包，从 CAN 帧提取信号值。**DBC 文件就是 COM 层的配置文件**。

你的项目中，这部分代码是手工实现而非 AUTOSAR 工具链自动生成（因为 AUTOSAR 商业工具授权费极高），但设计思想完全一致。`tools/dbc_generate_header.py` 从 JSON 生成 C 头文件 + DBC，模拟了 AUTOSAR 代码生成流程。

---

## 五、E2E Protection——你项目里的具体实践

### 5.1 AUTOSAR E2E 要防什么

CAN 总线的通信链路中，有三类问题硬件 CRC 管不了：

| 问题 | 场景 | 硬件 CRC 能管吗？ |
|---|---|---|
| **数据篡改** | SWC A 写了一个信号，COM 层打包时出错（如字节序搞反） | ❌ 硬件 CRC 只校验 CAN 帧传输过程，不管 SWC 逻辑错误 |
| **数据重复** | 发送方因 bug 重复发了同一帧 | ❌ |
| **数据丢失** | CAN 帧被 CRC 丢弃 → 接收方还在用旧数据 | ❌ 接收方不知道曾经有过这一帧 |
| **数据错序** | 两帧先后发出但因仲裁延迟导致接收顺序颠倒 | ❌ |
| **非预期接收** | 错误的 SWC 订阅了不该接收的消息 | ❌ |

**E2E（End-to-End）保护的思路是**：在 SWC 层（应用层）再加一层校验，从"发送方 SWC"到"接收方 SWC"端到端保护，不依赖中间传输层的可靠性假设。

### 5.2 AUTOSAR E2E 的四种 Profile

AUTOSAR 定义了 4 种 E2E Profile，安全性递增：

| Profile | 保护机制 | 开销 | 适用场景 |
|---|---|---|---|
| **Profile 1** | Counter + CRC | 额外 2 字节 | 低安全需求（如空调控制） |
| **Profile 2** | Counter + CRC + Data ID | 同上 | 中安全需求 |
| **Profile 4** | Counter + CRC + Data ID + Length | 同上 | 对标 ISO 26262 |
| **Profile 5** | CRC only (无 counter) | 额外 1 字节 | 最低需求 |

### 5.3 你的项目实现了 Profile 1 的精简版

对照 AUTOSAR E2E Profile 1 的标准要求：

| AUTOSAR E2E Profile 1 要求 | 你的实现 | 格式 |
|---|---|---|
| **Counter**（4-bit, 每帧+1, 接收方检测跳跃） | `flags[7:4]` 滚动计数器，每个信号独立 | `CAN_GET_COUNTER(flags)` |
| **CRC**（8-bit, 覆盖 Counter + Data） | 未实现，依赖 CAN 硬件 CRC | — |
| **Data ID**（标识"这是温度信号"还是"转速信号"） | 依赖 CAN ID 区分（0x200=温度, 0x183=转速） | `frame.id` |
| **Timeout**（接收方周期检查数据是否过期） | 心跳超时 3s + validity flag | `CAN_HEARTBEAT_TIMEOUT_MS` |

**面试回答话术**：

> "我参考了 AUTOSAR E2E Profile 1 的设计思路，在自己项目的应用层加了三层保护。第一，flags 字节 bit0 是 valid 标志——传感器物理坏了就清 0，接收方不会把旧数据当新数据显示。第二，flags 高 4-bit 是滚动计数器——每个信号独立递增，接收方如果看到 3→5 就知道第 4 帧丢了。第三，取消独立心跳帧——任一上报帧到达就是心跳，3 秒收不到任何帧触发离线告警。这三层合起来实现了 E2E 的核心诉求——不信任传输层一定会送达，在应用层做端到端验证。"

---

## 六、AUTOSAR 和你的项目对照

| AUTOSAR 概念 | 你的项目等同物 |
|---|---|
| SWC (软件组件) | `HomePage`, `rearViewCamera`, `ToolsPage` 等页面 |
| VFB / RTE | `rte.h/cpp` — `Rte::instance()`, `setMode(CAN/MOCK)`, 9 种业务信号 |
| COM Stack | `com_signal_defs.h` + `com_layer.c` + `comlayer.h/cpp` |
| 代码生成 | `tools/dbc_generate_header.py` — JSON→.h+.dbc 一键生成 |
| DBC 配置 | `smart_cockpit.dbc` (自动生成) |
| E2E Profile 1 | flags 字节 (counter + valid) |
| NvM (非易失存储) | `ConfigManager` (QSettings) |
| Diagnostic Stack | `CAN_ID_DIAGNOSTIC` (TEC/REC 上报) |

**你不是在用 AUTOSAR——但你已经实现了其中一个微型子集**。面试时这样表述比"我用了 AUTOSAR"更准确、更有说服力。

---

## 七、AUTOSAR 值得学吗

**如果你走汽车嵌入式方向——必学。** 国内主机厂（蔚小理、比亚迪、吉利）和 Tier 1 供应商（博世、大陆、联合电子）全部在用 AUTOSAR，招聘 JD 上大概率会写"熟悉 AUTOSAR 架构"。

入门路径：

1. **理解分层思想**（SWC / RTE / BSW）——你读完本文已经完成了
2. **深入 COM Stack**（信号→PDU→CAN 帧的完整路径）——对照你的 `can_task.c` 去理解
3. **看 AUTOSAR 官方文档**——`AUTOSAR_SWS_COM.pdf` 和 `AUTOSAR_SWS_E2E.pdf`
4. **如果有条件**，用 Vector DaVinci Developer 或 EB tresos 做一个最小 CAN 通信 Demo

**你已经有 DBC 文件和完整的 CAN 实践了——这是理解 AUTOSAR COM Stack 的最佳跳板。**

---

## 参考资料

- [AUTOSAR Classic Platform 官方文档](https://www.autosar.org/standards/classic-platform/)
- [AUTOSAR SWS E2E Protocol Specification](https://www.autosar.org/fileadmin/standards/R21-11/CP/AUTOSAR_SWS_E2EProtocol.pdf)
- [AUTOSAR SWS COM Stack Specification](https://www.autosar.org/fileadmin/standards/R21-11/CP/AUTOSAR_SWS_COM.pdf)
- 本项目：[CAN 总线入门指南](CAN总线入门指南.md)
- 本项目：[CAN 总线企业级改造总结](CAN总线企业级改造总结.md)
