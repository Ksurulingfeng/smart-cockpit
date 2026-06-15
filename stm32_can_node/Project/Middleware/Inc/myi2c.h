#ifndef __MYI2C_H__
#define __MYI2C_H__

#include "sys.h"

typedef struct {
    void *scl_port;    // SCL端口
    uint16_t scl_pin;  // SCL引脚
    void *sda_port;    // SDA端口
    uint16_t sda_pin;  // SDA引脚
    uint32_t delay_us; // I2C时钟延迟（微秒）
    uint8_t addr;      // I2C设备地址
} MyI2C_Device;

//========== 对外接口 ==========
void MyI2C_Init(MyI2C_Device *dev);
uint8_t MyI2C_Write(MyI2C_Device *dev, uint8_t *data, uint8_t len);
uint8_t MyI2C_Read(MyI2C_Device *dev, uint8_t *data, uint8_t len);
uint8_t MyI2C_WriteReg(MyI2C_Device *dev, uint8_t reg, uint8_t data);
uint8_t MyI2C_ReadReg(MyI2C_Device *dev, uint8_t reg, uint8_t *data);

#endif
