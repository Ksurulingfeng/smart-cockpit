#ifndef __ULTRASONIC_DRV_H__
#define __ULTRASONIC_DRV_H__

#include "sys.h"

#define ULTRANSONIC_MAX_DISTANCE_CM 400 // 最大量程cm
#define ULTRASONIC_TIMEOUT_MS       1000 // 测量超时时间ms

void Ultrasonic_Init(void);
void Ultrasonic_StartMeasure(void);
uint8_t Ultrasonic_IsMeasured(void);
float Ultrasonic_GetDistance(void);
void Ultrasonic_TIM_IRQCallback(void);

#endif /* __ULTRASONIC_DRV_H__ */