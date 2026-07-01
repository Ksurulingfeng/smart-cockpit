#ifndef __ADC_DRV_H__
#define __ADC_DRV_H__

#include "sys.h"

#define ADC_VREF              3.3f // ADC参考电压
#define ADC_RESOLUTION        4095 // ADC最大值
#define ADC_DIGITAL_THRESHOLD 2047 // ADC数字阈值

uint32_t ADC_ReadChannel(ADC_HandleTypeDef *hadc, uint32_t channel);
uint8_t ADC_ReadDigital(ADC_HandleTypeDef *hadc, uint32_t channel);
float ADC_ReadPercent(ADC_HandleTypeDef *hadc, uint32_t channel);
float ADC_ReadVoltage(ADC_HandleTypeDef *hadc, uint32_t channel);

#endif /* __ADC_DRV_H__ */
