# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

请始终使用简体中文思考和回答，并在回答时保持专业、简洁，除非代码或专有名词等特定需要。

## 项目概述

基于STM32F103C8Tx（"Blue Pill"）的FreeRTOS固件，实现双节点CAN总线仿真系统。代码库通过预处理器宏编译为两种固件变体之一：**节点A**（动力域，ID 0x1xx）或**节点B**（车身域，ID 0x2xx）。

## 构建系统

本项目使用VS Code的**EIDE（Embedded IDE）**插件，封装Keil MDK的**ARM Compiler 6（AC6）**。没有Makefile或CMake。

- **构建：** VS Code任务 `eide.project.build`（或 `Ctrl+Shift+B`）
- **烧录：** VS Code任务 `eide.project.uploadToDevice`
- **重新构建：** `eide.project.rebuild`
- **清理：** `eide.project.clean`

另外还在 `MDK-ARM/stm32_can_node.uvprojx` 提供了Keil MDK项目。

构建配置位于 `build/stm32_can_node/builder.params`（JSON格式）。关键设置：
- 工具链路径：`D:\ProgrammingLanguage\Embedded\Keil\Keil MDK-ARM\ARM\ARMCLANG`
- C标准：C99，优化级别 level-0，启用microlib
- MCU标志：Cortex-M3，无FPU
- 宏定义：`USE_HAL_DRIVER`、`STM32F103xB`、`NODE_B`（切换为 `NODE_A` 编译另一变体）
- ROM：64 KB，RAM：20 KB

## 调试

通过CMSIS-DAP使用Cortex-Debug + OpenOCD。启动配置在 `.vscode/launch.json` 中，加载 `build/stm32_can_node/` 下的 `.hex` 和 `.axf` 文件。

`printf` 输出通过自定义 `fputc` 重定向到USART1（PA9/PA10，115200 8N1）。调试打印使用 `debug_printf()` 宏，由 `Middleware/Inc/debug.h` 中的 `DEBUG_ENABLE` 控制开关。

## 架构

### 启动流程

`Reset_Handler` → `main()` → `System_Init()`（位于 [Core/Src/init.c](Core/Src/init.c)）→ FreeRTOS调度器。

`System_Init()` 初始化HAL、72 MHz时钟（8 MHz HSE × 9 PLL）、所有外设、BSP驱动、6个FreeRTOS队列、7个任务，然后调用 `vTaskStartScheduler()`。

### FreeRTOS任务（定义在 [Tasks/Inc/task_config.h](Tasks/Inc/task_config.h)）

| 任务 | 优先级 | 周期 | 功能 |
|---|---|---|---|
| CAN | 4（最高） | 20 ms | 周期发送状态帧，从队列接收并处理CAN消息 |
| ADC | 3 | 50 ms | 读取ADC1通道（PA0/PA1），滑动窗口均值滤波，设置全局变量 `g_rpm_percent`/`g_fuel_percent` |
| 超声波 | 3 | 500 ms | 触发HC-SR04，等待TIM3捕获ISR发出的任务通知 |
| 执行器 | 2 | 20 ms | 蜂鸣器状态机、LED心跳、电机转速、舵机角度（从蜂鸣器/LED/风扇/车窗队列读取） |
| 温湿度 | 2 | 2 s | 读取DHT11温湿度传感器 |
| 按键 | 2 | 20 ms | 3个按键（PC13-15）和档位选择器（PA6-7）去抖动，发送事件到按键队列 |
| 显示 | 1（最低） | 200 ms | 2页OLED（SSD1306 128×32），显示传感器数据 |

监控任务（优先级1，周期5 s）已编写但在 `init.c` 中被注释掉——其功能为报告FreeRTOS栈高水位。

### 任务间通信

所有通信均通过FreeRTOS队列进行——任务之间不使用共享全局变量（传感器读数 `g_rpm_percent`、`g_fuel_percent`、`g_temperature_x10`、`g_humidity_x10`、`g_ultrasonic_distance` 除外）。

6个队列：`xCanRxQueue`、`xBuzzerQueue`、`xFanQueue`、`xKeyQueue`、`xLEDQueue`、`xWindowQueue`。

### CAN接收路径（ISR → 队列 → 任务）

[Core/Src/can.c](Core/Src/can.c) 中的 `HAL_CAN_RxFifo0MsgPendingCallback` 调用 `can_drv.c` 中注册的回调函数，该回调通过 `xQueueSendFromISR` 将 `CanRxMsg_t` 发送到 `xCanRxQueue`。CAN任务出队并按CAN ID分发到相应的执行器队列。

### 超声波路径（ISR → 任务通知）

TIM3_CH2（PB5）输入捕获测量HC-SR04回波脉宽。[Core/Src/stm32f1xx_it.c](Core/Src/stm32f1xx_it.c) 中的TIM3 ISR调用 `vTaskNotifyGiveFromISR` 唤醒超声波任务，然后读取距离值。

### CAN协议（[Tasks/Inc/can_protocol.h](Tasks/Inc/can_protocol.h)）

CAN1工作在250 kbps（APB1=36 MHz，预分频=18，BS1=4TQ，BS2=3TQ）。滤波器接受所有ID。通过 `#ifdef NODE_A` / `#ifdef NODE_B` 控制节点行为。

- **节点A** 发送：状态（0x100）、超声波距离（0x101）、档位（0x102）、油量（0x103）、转速（0x104）。接收：蜂鸣器（0x180）、LED（0x181）。
- **节点B** 发送：状态（0x200）、温度（0x201）、湿度（0x202）、风扇转速（0x203）、车窗位置（0x204）。接收：风扇目标转速（0x280）、车窗目标位置（0x281）、蜂鸣器（0x282）、LED（0x283）。

### 关键抽象层

- **BSP驱动**（[Drivers/BSP/](Drivers/BSP/)）以简洁的API封装硬件外设：`CAN_SendMessage()`、`LED_On()`、`Buzzer_Beep()`、`Motor_SetSpeed()`、`Servo_SetAngle()`、`DHT11_Read_Data()`、`Ultrasonic_GetDistance()`。
- **软件I2C**（[Middleware/Src/myi2c.c](Middleware/Src/myi2c.c)）在PB6(SCL)/PB7(SDA)上通过位操作模拟I2C驱动OLED——未使用硬件I2C。
- **DWT延时**（[Core/Src/delay.c](Core/Src/delay.c)）利用Cortex-M3周期计数器实现微秒级精确延时。
- **HAL时基**（[Core/Src/stm32f1xx_hal_timebase_tim.c](Core/Src/stm32f1xx_hal_timebase_tim.c)）运行在TIM4上而非SysTick，将SysTick留给FreeRTOS使用。

## 切换节点A与节点B

修改 `build/stm32_can_node/builder.params` 第34行的宏定义：将 `NODE_B` 改为 `NODE_A`（或反之），然后重新构建。任务源文件（主要是 `can_task.c`、`actuator_task.c`、`display_task.c`）中的 `#ifdef` 分支会选择编译对应的CAN ID和逻辑。

## CubeMX

`.ioc` 文件（[stm32_can_node.ioc](stm32_can_node.ioc)）是一个CubeMX 6.15.0项目。从中重新生成代码会覆盖 `Core/Src/` 和 `Core/Inc/` 中的HAL初始化文件。任何在 `USER CODE BEGIN`/`USER CODE END` 注释块之外添加的自定义代码将会丢失。
