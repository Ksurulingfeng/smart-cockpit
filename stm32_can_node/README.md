# STM32 CAN 节点固件——分布式车身感知与执行层

基于 STM32F103C8Tx 的 FreeRTOS 固件，通过 `#ifdef NODE_A`/`NODE_B` 编译为两套变体，分别模拟动力域 ECU（节点A）和车身域 ECU（节点B）。

## 功能

| 模块 | 节点 A（动力域） | 节点 B（车身域） |
|---|---|---|
| CAN 上报 | 0x180-0x183（超声波/档位/油量/转速） | 0x200-0x203（温湿度/风扇反馈/车窗反馈） |
| CAN 接收 | 0x080-0x081（蜂鸣器/LED 控制） | 0x100-0x103（风扇/车窗/蜂鸣/LED 控制） |
| 传感器 | ADC 转速/油量、HC-SR04 超声波、档位选择器 | DHT11 温湿度 |
| 执行器 | 蜂鸣器、LED | 风扇(电机)、车窗(舵机)、蜂鸣器、LED |
| 按键 | PC13-15 × 3 | PC13-15 × 3 |
| 显示屏 | SSD1306 OLED 128×32 | SSD1306 OLED 128×32 |
| 诊断 | 0x300 DiagReport (TEC/REC/总线负载，10s) | 同左 |

## 传感器与执行器

- **ADC**: PA0 (转速), PA1 (油量), 滑动窗口均值滤波
- **超声波**: HC-SR04, TIM3_CH2 (PB5) 输入捕获, 带有效性标志
- **温湿度**: DHT11 单总线, 读失败时清 valid 标志通知主控
- **档位**: 三档拨动开关, P=0 (防御性: 悬空默认驻车) / R=1 / D=2
- **蜂鸣器**: 有源蜂鸣器, 关/单次/连续/报警 4 模式
- **LED**: PA8, 常亮/心跳/报警 3 模式
- **风扇**: L9110S 直流电机, TIM2_CH2 PWM 0-100%, 实际值反馈上报
- **车窗**: SG90 舵机, TIM2_CH1 0-100%, 实际位置反馈上报
- **OLED**: SSD1306 128×32, 软件 I2C (PB6 SCL/PB7 SDA)

## 构建

| 项目 | 值 |
|---|---|
| IDE | VS Code + EIDE 插件 |
| 工具链 | Keil MDK ARM Compiler 6 (AC6) |
| MCU | STM32F103C8Tx (Cortex-M3, 64KB/20KB) |
| C 标准 | C99, level-0, microlib |
| 宏定义 | `USE_HAL_DRIVER, STM32F103xB, NODE_A\|NODE_B` |
| CAN | 250kbps (APB1=36MHz, Prescaler=9, BS1=12TQ, BS2=3TQ, 采样点 81.25%) |

**构建步骤**: VS Code 打开目录 → `Ctrl+Shift+B` → EIDE 任务烧录

## FreeRTOS 架构

| 任务 | 优先级 | 周期 | 功能 |
|---|---|---|---|
| CAN | 4 | 20ms | 周期发送传感器帧+执行器反馈+诊断帧, 收发 CAN 消息 |
| ADC | 3 | 50ms | ADC1 CH0/CH1 均值滤波 |
| 超声波 | 3 | 500ms | HC-SR04 触发+TaskNotify 等待, 含越界检测 |
| 执行器 | 2 | 20ms | 蜂鸣器状态机/LED 心跳/电机/舵机, 记录实际值到全局变量 |
| 温湿度 | 2 | 2s | DHT11 读取, 成功/失败时设置 valid 标志 |
| 按键 | 2 | 20ms | 3 按键+档位选择器去抖 |
| 显示 | 1 | 200ms | OLED 2 页刷新 |

## CAN 容错特性

- **COM 层双向**: `Com_SendSignal(SID, value)` 发送, `Com_ReceiveSignal(SID)` 接收, 信号↔CAN帧自动打包/解包
- **DLC 保护**: ISR 中 `if(len>8) len=8`, 接收端 `if(len<1) return` 防截断帧
- **发送超时**: Com_Flush/Com_SendRaw 100 次重试后 taskYIELD, 防最高优先级死锁
- **BusOff 自动恢复**: AutoBusOff=ENABLE, 错误回调跟踪 TEC/REC
- **精确滤波器**: 节点A 收 0x080-0x081+0x7E0, 节点B 收 0x100-0x103+0x7E0, 硬件过滤
- **UDS 诊断 (ISO 14229-1)**: 支持 0x10(会话控制)/0x22(读DID)/0x3E(会话保持)/0x11(ECU复位), S3 超时自动切回默认会话
- **滚动计数器**: 每个传感器信号独立 4-bit counter, 主控可检测丢帧
- **执行器反馈**: 风扇实际转速+车窗实际位置 每 100ms 上报, 形成控制闭环

## 调试

- CMSIS-DAP + Cortex-Debug + OpenOCD (`.vscode/launch.json`)
- USART1 (PA9/PA10, 115200 8N1): `debug_printf()` 宏

## CAN 协议

协议源 [tools/can_signals.json](../tools/can_signals.json)，生成 `python tools/dbc_generate_header.py`。
输出: [can_protocol_common.h](../shared/can_protocol_common.h) + [signal_id.h](../tools/signal_id.h) + [DBC](../shared/smart_cockpit.dbc)。
