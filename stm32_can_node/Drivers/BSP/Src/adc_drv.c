#include "adc_drv.h"

uint32_t ADC_ReadChannel(ADC_HandleTypeDef *hadc, uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    sConfig.Channel      = channel;
    sConfig.Rank         = 1; // 单次转换只有一路
    sConfig.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;
    HAL_ADC_ConfigChannel(hadc, &sConfig);

    HAL_ADC_Start(hadc);
    HAL_ADC_PollForConversion(hadc, HAL_MAX_DELAY);
    uint32_t value = HAL_ADC_GetValue(hadc);
    HAL_ADC_Stop(hadc);

    return value;
}

uint8_t ADC_ReadDigital(ADC_HandleTypeDef *hadc, uint32_t channel)
{
    uint32_t adcValue = ADC_ReadChannel(hadc, channel);

    // 假设使用12位ADC，参考电压为3.3V，数字阈值为1.65V
    return (adcValue > ADC_DIGITAL_THRESHOLD) ? 1 : 0;
}

float ADC_ReadPercent(ADC_HandleTypeDef *hadc, uint32_t channel)
{
    uint32_t adcValue = ADC_ReadChannel(hadc, channel);

    // 将ADC值转换为百分比
    return (adcValue * 100.0f) / (float)ADC_RESOLUTION;
}

float ADC_ReadVoltage(ADC_HandleTypeDef *hadc, uint32_t channel)
{
    uint32_t adcValue = ADC_ReadChannel(hadc, channel);

    // 假设使用12位ADC，参考电压为3.3V
    float voltage = (adcValue / (float)ADC_RESOLUTION) * ADC_VREF;
    return voltage;
}