#include "dht11_drv.h"
#include "debug.h"

/* 宏定义，用于简化代码，方便切换GPIO模式 */
#define DHT11_DATA_LOW()  HAL_GPIO_WritePin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin, GPIO_PIN_RESET)
#define DHT11_DATA_HIGH() HAL_GPIO_WritePin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin, GPIO_PIN_SET)
#define DHT11_DATA_READ() HAL_GPIO_ReadPin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin)

/* 设置GPIO为输出模式 */
static void DHT11_Mode_Out(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin              = DHT11_DATA_Pin;
    GPIO_InitStruct.Mode             = GPIO_MODE_OUTPUT_PP; // 推挽输出
    GPIO_InitStruct.Pull             = GPIO_NOPULL;
    GPIO_InitStruct.Speed            = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT11_DATA_GPIO_Port, &GPIO_InitStruct);
}

/* 设置GPIO为输入模式 */
static void DHT11_Mode_In(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin              = DHT11_DATA_Pin;
    GPIO_InitStruct.Mode             = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull             = GPIO_PULLUP; // 上拉输入
    HAL_GPIO_Init(DHT11_DATA_GPIO_Port, &GPIO_InitStruct);
}

/* 初始化DHT11引脚 */
void DHT11_Init(void)
{
    DHT11_Mode_Out();  // 初始为输出模式
    DHT11_DATA_HIGH(); // 默认总线为高电平（空闲状态）
    delay_ms(100);
}

/* 等待引脚变为指定电平，超时返回0，成功返回1 */
static uint8_t DHT11_Wait_For_Level(uint8_t level, uint32_t timeout_us)
{
    uint32_t cnt = 0;
    while (DHT11_DATA_READ() != level) {
        cnt++;
        if (cnt > timeout_us) return 0;
        delay_us(1);
    }
    return 1;
}

/* 等待引脚变为低电平（超时us） */
#define DHT11_Wait_Low(timeout_us) DHT11_Wait_For_Level(GPIO_PIN_RESET, timeout_us)
/* 等待引脚变为高电平（超时us） */
#define DHT11_Wait_High(timeout_us) DHT11_Wait_For_Level(GPIO_PIN_SET, timeout_us)

/* 从DHT11读取一个字节的数据 */
static uint8_t DHT11_Read_Byte(uint32_t timeout_us)
{
    uint8_t i, data = 0;

    for (i = 0; i < 8; i++) {
        // 等待低电平（表示一个bit开始）
        if (!DHT11_Wait_Low(timeout_us)) return 0;
        // 等待低电平结束（固定50us）
        if (!DHT11_Wait_High(timeout_us)) return 0;

        // 延时40us后判断电平
        delay_us(40);
        if (DHT11_DATA_READ() == GPIO_PIN_SET) {
            data |= (1 << (7 - i));
            // 等待剩余的高电平结束（如果未结束）
            if (!DHT11_Wait_High(timeout_us)) return 0;
        }
    }
    return data;
}

/* 读取一次DHT11的温湿度数据
 * 返回值: 0: 成功, 1: 失败
 * 参数: 指向存储温度和湿度的浮点数指针
 */
uint8_t DHT11_Read_Data(float *temperature, float *humidity)
{
    uint8_t buffer[5]   = {0};
    uint32_t timeout_us = 50000; // 总超时50ms

    // ---- 发送起始信号（不变）----
    DHT11_Mode_Out();
    DHT11_DATA_LOW();
    delay_ms(20);
    DHT11_DATA_HIGH();
    delay_us(30);

    // ---- 等待响应（已有超时可保留）----
    DHT11_Mode_In();
    uint32_t timeout = 10000;
    while (DHT11_DATA_READ() == GPIO_PIN_SET && timeout--);
    if (timeout == 0) return 1;

    timeout = 10000;
    while (DHT11_DATA_READ() == GPIO_PIN_RESET && timeout--);
    if (timeout == 0) return 1;

    // ---- 读取5个字节，每个字节带超时----
    for (int i = 0; i < 5; i++) {
        uint8_t byte = DHT11_Read_Byte(timeout_us);
        if (byte == 0 && i == 0) return 1; // 简单失败判断
        buffer[i] = byte;
    }

    DHT11_Mode_Out();
    DHT11_DATA_HIGH();

    if (buffer[4] != (buffer[0] + buffer[1] + buffer[2] + buffer[3]))
        return 1;

    *humidity    = (float)(buffer[0] + buffer[1] / 10.0);
    *temperature = (float)(buffer[2] + buffer[3] / 10.0);
    return 0;
}

void DHT11_Simple_Test(void)
{
    uint8_t bits[40] = {0};

    // 发送起始信号
    DHT11_Mode_Out();
    DHT11_DATA_LOW();
    delay_ms(20);
    DHT11_DATA_HIGH();
    delay_us(30);

    // 切换输入
    DHT11_Mode_In();

    // 等待响应
    while (DHT11_DATA_READ() == GPIO_PIN_SET);
    while (DHT11_DATA_READ() == GPIO_PIN_RESET);

    // 读取40位数据
    for (int i = 0; i < 40; i++) {
        while (DHT11_DATA_READ() == GPIO_PIN_SET);   // 等待低电平开始
        while (DHT11_DATA_READ() == GPIO_PIN_RESET); // 等待低电平结束
        delay_us(40);                                // 40us后判断
        bits[i] = DHT11_DATA_READ();                 // 1 或 0
        // 如果读到1，需要等待高电平结束
        if (bits[i]) {
            while (DHT11_DATA_READ() == GPIO_PIN_SET);
        }
    }

    // 恢复总线
    DHT11_Mode_Out();
    DHT11_DATA_HIGH();

    // 打印原始数据
    for (int i = 0; i < 40; i++) {
        printf("%d", bits[i]);
        if ((i + 1) % 8 == 0) printf(" ");
    }
    printf("\r\n");
}