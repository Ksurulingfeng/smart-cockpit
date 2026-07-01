#ifndef FREERTOSCONFIG
#define FREERTOSCONFIG

// 调度器配置
#define configUSE_PREEMPTION 1 // 使用抢占式调度器
#define configUSE_IDLE_HOOK  0 // 不使用空闲任务钩子函数
#define configUSE_TICK_HOOK  0 // 不使用时钟节拍钩子函数

// 系统时钟配置
#define configCPU_CLOCK_HZ ((unsigned long)72000000) // CPU时钟频率72MHz
#define configTICK_RATE_HZ ((TickType_t)1000)        // 系统节拍频率1000Hz，即1ms一个节拍

// 任务配置
#define configMAX_PRIORITIES     (10)                  // 最大任务优先级
#define configMINIMAL_STACK_SIZE ((unsigned short)128) // 最小任务栈大小
#define configTOTAL_HEAP_SIZE    ((size_t)(8 * 1024))  // 堆总大小8KB
#define configMAX_TASK_NAME_LEN  (16)                  // 任务名称最大长度

// 软件定时器配置
#define configUSE_TIMERS             1                              // 使用软件定时器
#define configTIMER_TASK_PRIORITY    (configMAX_PRIORITIES - 1)     // 软件定时器任务优先级
#define configTIMER_QUEUE_LENGTH     10                             // 软件定时器队列长度
#define configTIMER_TASK_STACK_DEPTH (configMINIMAL_STACK_SIZE * 2) // 软件定时器任务栈深度

// 调试和统计功能
#define configUSE_TRACE_FACILITY             1 // 启用可视化跟踪调试
#define configUSE_16_BIT_TICKS               0 // 不使用16位节拍计数器
#define configIDLE_SHOULD_YIELD              1 // 空闲任务让出CPU
#define configUSE_STATS_FORMATTING_FUNCTIONS 1 // 启用统计格式函数

// 同步机制配置
#define configUSE_MUTEXES             1 // 使用互斥信号量
#define configUSE_COUNTING_SEMAPHORES 1 // 使用计数信号量

// 协程配置（已废弃，通常设为0）
#define configUSE_CO_ROUTINES           0   // 不使用协程
#define configMAX_CO_ROUTINE_PRIORITIES (2) // 协程最大优先级数量

// 包含的API函数配置
#define INCLUDE_vTaskPrioritySet            1 // 任务优先级设置函数
#define INCLUDE_uxTaskPriorityGet           1 // 获取任务优先级函数
#define INCLUDE_vTaskDelete                 1 // 删除任务函数
#define INCLUDE_vTaskCleanUpResources       0 // 清理资源函数
#define INCLUDE_vTaskSuspend                1 // 挂起任务函数
#define INCLUDE_vTaskDelayUntil             1 // 绝对延时函数
#define INCLUDE_vTaskDelay                  1 // 相对延时函数
#define INCLUDE_xTaskGetSchedulerState      1 // 获取调度器状态函数
#define INCLUDE_xTaskGetHandle              1 // 获取任务句柄函数
#define INCLUDE_uxTaskGetStackHighWaterMark 1 // 获取任务栈高水位线函数
#define INCLUDE_eTaskGetState               1 // 获取任务状态函数

// 中断优先级配置 - Cortex-M内核
#define configKERNEL_INTERRUPT_PRIORITY      255 // 内核中断优先级（最低优先级）
#define configMAX_SYSCALL_INTERRUPT_PRIORITY 191 // 可安全调用FreeRTOS API的最高中断优先级
                                                 // 191对应优先级11（0xb0）

// STM32库中断优先级配置（0-15级）
#define configLIBRARY_KERNEL_INTERRUPT_PRIORITY 15 // 库函数中的内核中断优先级（最低）

// 中断服务程序重命名
#define xPortPendSVHandler PendSV_Handler // PendSV中断服务程序
#define vPortSVCHandler    SVC_Handler    // SVC中断服务程序

#endif /* FREERTOSCONFIG */
