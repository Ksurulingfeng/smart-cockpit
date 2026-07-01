#include "servo_drv.h"

void Servo_Init(void)
{
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);
}

void Servo_SetAngle(float Angle)
{
    uint16_t pulse = (uint16_t)(Angle / 180 * 2000 + 500);
    __HAL_TIM_SetCompare(&htim2, TIM_CHANNEL_4, pulse);
}