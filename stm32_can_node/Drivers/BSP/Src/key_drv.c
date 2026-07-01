#include "key_drv.h"
#include "delay.h"

uint8_t Key_GetState(void)
{
    if (!HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin)) return KEY_NEXT;
    if (!HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin)) return KEY_PREV;
    if (!HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin)) return KEY_ENTER;
    return KEY_NONE;
}

uint8_t Gear_GetState(void)
{
    uint8_t a = HAL_GPIO_ReadPin(GEAR_A_GPIO_Port, GEAR_A_Pin);
    uint8_t b = HAL_GPIO_ReadPin(GEAR_B_GPIO_Port, GEAR_B_Pin);
    if (!a && b) return GEAR_D;
    if (a && b) return GEAR_P;
    if (a && !b) return GEAR_R;
    return GEAR_NONE;
}