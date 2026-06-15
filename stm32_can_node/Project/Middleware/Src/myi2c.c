#include "myi2c.h"
#include "delay.h"

//========== 底层硬件操作 ==========
static void pin_write(void *port, uint16_t pin, uint8_t val)
{
    GPIO_TypeDef *gpio = (GPIO_TypeDef *)port;
    HAL_GPIO_WritePin(gpio, pin, val ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static uint8_t pin_read(void *port, uint16_t pin)
{
    GPIO_TypeDef *gpio = (GPIO_TypeDef *)port;
    return HAL_GPIO_ReadPin(gpio, pin) == GPIO_PIN_SET ? 1 : 0;
}

//========== I2C时序操作 ==========
static void i2c_delay(MyI2C_Device *dev)
{
    delay_us(dev->delay_us);
}

static void scl_set(MyI2C_Device *dev, uint8_t val)
{
    pin_write(dev->scl_port, dev->scl_pin, val);
}

static void sda_set(MyI2C_Device *dev, uint8_t val)
{
    pin_write(dev->sda_port, dev->sda_pin, val);
}

static uint8_t sda_read(MyI2C_Device *dev)
{
    return pin_read(dev->sda_port, dev->sda_pin);
}

static void i2c_start(MyI2C_Device *dev)
{
    sda_set(dev, 1);
    scl_set(dev, 1);
    i2c_delay(dev);
    sda_set(dev, 0);
    i2c_delay(dev);
    scl_set(dev, 0);
}

static void i2c_stop(MyI2C_Device *dev)
{
    sda_set(dev, 0);
    scl_set(dev, 1);
    i2c_delay(dev);
    sda_set(dev, 1);
    i2c_delay(dev);
}

static void i2c_send_byte(MyI2C_Device *dev, uint8_t data)
{
    for (uint8_t i = 0; i < 8; i++) {
        sda_set(dev, (data & 0x80) ? 1 : 0);
        data <<= 1;
        scl_set(dev, 1);
        i2c_delay(dev);
        scl_set(dev, 0);
        i2c_delay(dev);
    }
    sda_set(dev, 1); // 释放SDA准备接收ACK
}

static uint8_t i2c_recv_byte(MyI2C_Device *dev, uint8_t ack)
{
    uint8_t data = 0;
    sda_set(dev, 1); // 释放SDA

    for (uint8_t i = 0; i < 8; i++) {
        data <<= 1;
        scl_set(dev, 1);
        i2c_delay(dev);
        if (sda_read(dev)) data |= 1;
        scl_set(dev, 0);
        i2c_delay(dev);
    }

    // 发送ACK(0)或NACK(1)
    sda_set(dev, ack ? 0 : 1);
    scl_set(dev, 1);
    i2c_delay(dev);
    scl_set(dev, 0);
    sda_set(dev, 1);

    return data;
}

static uint8_t i2c_wait_ack(MyI2C_Device *dev)
{
    uint16_t timeout = 1000;
    scl_set(dev, 1);
    i2c_delay(dev);

    while (sda_read(dev) && timeout--) {
        i2c_delay(dev);
    }

    scl_set(dev, 0);
    return timeout > 0;
}

//========== 对外接口 ==========
// 初始化I2C设备
void MyI2C_Init(MyI2C_Device *dev)
{
    scl_set(dev, 1);
    sda_set(dev, 1);
}

// 写数据
uint8_t MyI2C_Write(MyI2C_Device *dev, uint8_t *data, uint8_t len)
{
    i2c_start(dev);

    // 发送设备地址+写标志
    i2c_send_byte(dev, (dev->addr << 1) | 0);
    if (!i2c_wait_ack(dev)) {
        i2c_stop(dev);
        return 0;
    }

    // 发送数据
    for (uint8_t i = 0; i < len; i++) {
        i2c_send_byte(dev, data[i]);
        if (!i2c_wait_ack(dev)) {
            i2c_stop(dev);
            return 0;
        }
    }

    i2c_stop(dev);
    return 1; // 成功
}

// 读数据
uint8_t MyI2C_Read(MyI2C_Device *dev, uint8_t *data, uint8_t len)
{
    i2c_start(dev);

    // 发送设备地址+读标志
    i2c_send_byte(dev, (dev->addr << 1) | 1);
    if (!i2c_wait_ack(dev)) {
        i2c_stop(dev);
        return 0;
    }

    // 读取数据，最后一个字节发送NACK
    for (uint8_t i = 0; i < len; i++) {
        data[i] = i2c_recv_byte(dev, i < len - 1);
    }

    i2c_stop(dev);
    return 1; // 成功
}

// 写寄存器
uint8_t MyI2C_WriteReg(MyI2C_Device *dev, uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {reg, data};
    return MyI2C_Write(dev, buf, 2);
}

// 读寄存器
uint8_t MyI2C_ReadReg(MyI2C_Device *dev, uint8_t reg, uint8_t *data)
{
    // 先写入寄存器地址
    if (!MyI2C_Write(dev, &reg, 1)) {
        return 0;
    }
    // 再读取数据
    return MyI2C_Read(dev, data, 1);
}
