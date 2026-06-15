#ifndef __TASK_CONFIG_H
#define __TASK_CONFIG_H

/* ==================== 任务优先级定义 ==================== */
#define TASK_PRIO_ACTUATOR   2 // 执行器制任务
#define TASK_PRIO_ADC        3 // ADC采集任务
#define TASK_PRIO_CAN        4 // CAN通信任务
#define TASK_PRIO_DISPLAY    1 // 显示刷新任务
#define TASK_PRIO_HUMITURE   2 // 温湿度采集
#define TASK_PRIO_KEY        2 // 按键扫描任务
#define TASK_PRIO_MONITOR    1 // 监控任务
#define TASK_PRIO_ULTRASONIC 3 // 超声波任务

/* ==================== 任务栈大小定义 ==================== */
#define TASK_STACK_ACTUATOR   192 // 执行器制任务
#define TASK_STACK_ADC        128 // ADC采集任务
#define TASK_STACK_CAN        192 // CAN通信任务
#define TASK_STACK_DISPLAY    192 // 显示刷新任务
#define TASK_STACK_HUMITURE   128 // 温湿度采集
#define TASK_STACK_KEY        64  // 按键扫描任务
#define TASK_STACK_MONITOR    128 // 监控任务
#define TASK_STACK_ULTRASONIC 128 // 超声波任务

/* ==================== 任务周期定义（ms） ==================== */
#define TASK_CYCLE_ACTUATOR   20   // 执行器制任务周期
#define TASK_CYCLE_ADC        50   // ADC采集周期
#define TASK_CYCLE_CAN        20   // CAN收发周期
#define TASK_CYCLE_DISPLAY    200  // 显示刷新周期
#define TASK_CYCLE_HUMITURE   2000 // 温湿度采集周期
#define TASK_CYCLE_KEY        20   // 按键扫描周期
#define TASK_CYCLE_MONITOR    5000 // 监控任务周期
#define TASK_CYCLE_ULTRASONIC 500  // 超声波采集周期

#endif /* __TASK_CONFIG_H */