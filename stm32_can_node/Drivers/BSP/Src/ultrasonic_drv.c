#include "ultrasonic_drv.h"
#include "delay.h"
#include "task_headfile.h"

/* 模块内部状态 */
static uint32_t capture_rise          = 0;
static uint32_t capture_fall          = 0;
static volatile uint8_t capture_state = 0; // 0=等待上升沿, 1=等待下降沿
static uint32_t high_time_us          = 0;
static volatile uint8_t measure_done  = 0;
static volatile float distance_cm     = -1.0f;

void Ultrasonic_Init(void)
{
    HAL_TIM_IC_Stop_IT(&htim3, TIM_CHANNEL_2);
    capture_state = 0;
    measure_done  = 0;
    distance_cm   = -1.0f;
}

/* 启动测距（非阻塞） */
void Ultrasonic_StartMeasure(void)
{
    // 1. 停止前一次捕获，重置状态机
    HAL_TIM_IC_Stop_IT(&htim3, TIM_CHANNEL_2);
    capture_state = 0;
    measure_done  = 0;
    distance_cm   = -1.0f;

    // 清除残留CC2中断标志（否则HAL_TIM_IC_Start_IT会使能后立即触发虚假中断）
    __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_CC2);

    // 2. 产生 10us 以上高电平触发脉冲
    HAL_GPIO_WritePin(UT_TRIG_GPIO_Port, UT_TRIG_Pin, GPIO_PIN_SET);
    delay_us(15);
    HAL_GPIO_WritePin(UT_TRIG_GPIO_Port, UT_TRIG_Pin, GPIO_PIN_RESET);

    // 3. 配置捕获为上升沿，开启中断
    __HAL_TIM_SET_CAPTUREPOLARITY(&htim3, TIM_CHANNEL_2, TIM_INPUTCHANNELPOLARITY_RISING);
    HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_2);
}

/* 检查测量是否完成 */
uint8_t Ultrasonic_IsMeasured(void)
{
    return measure_done;
}

/* 获取距离（cm），需要在测量完成后调用 */
float Ultrasonic_GetDistance(void)
{
    if (measure_done) {
        return distance_cm;
    } else {
        return -1.0f;
    }
}

/* 定时器捕获中断回调（需由用户的中断处理函数中调用） */
void Ultrasonic_TIM_IRQCallback(void)
{
    /* 处理捕获中断 */
    if (capture_state == 0) { // 上升沿
        capture_rise = HAL_TIM_ReadCapturedValue(&htim3, TIM_CHANNEL_2);
        // 切换为下降沿捕获
        __HAL_TIM_SET_CAPTUREPOLARITY(&htim3, TIM_CHANNEL_2, TIM_INPUTCHANNELPOLARITY_FALLING);
        capture_state = 1;
    } else { // 下降沿
        capture_fall = HAL_TIM_ReadCapturedValue(&htim3, TIM_CHANNEL_2);

        // 计算高电平时间（处理单次溢出，假设无多次溢出）
        if (capture_fall >= capture_rise) {
            high_time_us = capture_fall - capture_rise;
        } else {
            // 计数器溢出过一次
            high_time_us = (0xFFFF - capture_rise + capture_fall + 1);
        }

        // 停止捕获
        HAL_TIM_IC_Stop_IT(&htim3, TIM_CHANNEL_2);
        capture_state = 0;

        // 计算距离 (cm) = 高电平时间(us) * 0.034 / 2
        // 0.034 cm/us 是20°C下的近似声速
        distance_cm = (float)high_time_us * 0.017f; // 等价于 (us*0.034/2)
        if (distance_cm < 2.0f || distance_cm > ULTRANSONIC_MAX_DISTANCE_CM) {
            distance_cm = -1.0f; // 超量程或噪声标记无效
        }
        measure_done = 1;

        // freertos任务通知
        BaseType_t higher_priority_task_woken = pdFALSE;
        vTaskNotifyGiveFromISR(xUltrasonicTaskHandle, &higher_priority_task_woken);
        portYIELD_FROM_ISR(higher_priority_task_woken);
    }
    // 可选：处理定时器溢出，增加测距范围（简单实现忽略多次溢出）
}