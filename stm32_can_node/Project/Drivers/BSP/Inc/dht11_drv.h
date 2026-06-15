#ifndef __DHT11_DRV_H__
#define __DHT11_DRV_H__

#include "sys.h"

void DHT11_Init(void);
uint8_t DHT11_Read_Data(float *temperature, float *humidity);
void DHT11_Simple_Test(void);

#endif /* __DHT11_DRV_H__ */
