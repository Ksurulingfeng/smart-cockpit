#include "oled_i2c_drv.h"
#include "oled_font.h"
#include "myi2c.h"

// 定义 OLED 的 I2C 设备实例
static MyI2C_Device g_oled_i2c_dev;

// 写字节
void OLED_WR_Byte(uint8_t data, uint8_t mode)
{
    if (mode == OLED_DATA) {
        MyI2C_WriteReg(&g_oled_i2c_dev, OLED_REG_DATA, data);
    } else {
        MyI2C_WriteReg(&g_oled_i2c_dev, OLED_REG_CMD, data);
    }
}

// 反色写字节
void OLED_WR_Byte_Inverse(uint8_t data, uint8_t mode)
{
    OLED_WR_Byte(~data, mode);
}

// 设置光标位置
void OLED_Set_Pos(uint8_t x, uint8_t y)
{
    OLED_WR_Byte(0xb0 + y, OLED_CMD);
    OLED_WR_Byte(((x & 0xf0) >> 4) | 0x10, OLED_CMD);
    OLED_WR_Byte((x & 0x0f), OLED_CMD);
}

// 开启显示
void OLED_Display_On(void)
{
    OLED_WR_Byte(0X8D, OLED_CMD);
    OLED_WR_Byte(0X14, OLED_CMD);
    OLED_WR_Byte(0XAF, OLED_CMD);
}

// 关闭显示
void OLED_Display_Off(void)
{
    OLED_WR_Byte(0X8D, OLED_CMD);
    OLED_WR_Byte(0X10, OLED_CMD);
    OLED_WR_Byte(0XAE, OLED_CMD);
}

// 全屏清空（黑色）
void OLED_Clear(void)
{
    for (uint8_t i = 0; i < 8; i++) {
        OLED_WR_Byte(0xb0 + i, OLED_CMD);
        OLED_WR_Byte(0x00, OLED_CMD);
        OLED_WR_Byte(0x10, OLED_CMD);
        for (uint8_t n = 0; n < 128; n++) {
            OLED_WR_Byte(0, OLED_DATA);
        }
    }
}

// 局部清屏
void OLED_Clear_Area(uint8_t x1, uint8_t x2, uint8_t y1, uint8_t y2)
{
    for (uint8_t page = y1; page < y2; page++) {
        OLED_WR_Byte(0xB0 + page, OLED_CMD);
        OLED_WR_Byte(((x1 & 0xF0) >> 4) | 0x10, OLED_CMD);
        OLED_WR_Byte((x1 & 0x0F), OLED_CMD);
        for (uint8_t col = x1; col < x2; col++) {
            OLED_WR_Byte(0, OLED_DATA);
        }
    }
}

// 全屏点亮（显示白色）
void OLED_On(void)
{
    for (uint8_t i = 0; i < 8; i++) {
        OLED_WR_Byte(0xb0 + i, OLED_CMD);
        OLED_WR_Byte(0x00, OLED_CMD);
        OLED_WR_Byte(0x10, OLED_CMD);
        for (uint8_t n = 0; n < 128; n++) {
            OLED_WR_Byte(0xFF, OLED_DATA);
        }
    }
}

// 显示单个字符
void OLED_ShowChar(uint8_t x, uint8_t y, const char chr, uint8_t Char_Size, uint8_t mode)
{
    uint8_t c = chr - ' ';
    if (x > Max_Column - 1) {
        x = 0;
        y += 2;
    }

    if (mode == OLED_MODE_INVERSE) {
        if (Char_Size == OLED_SIZE_8x16) {
            OLED_Set_Pos(x, y);
            for (uint8_t i = 0; i < 8; i++)
                OLED_WR_Byte_Inverse(F8X16[c * 16 + i], OLED_DATA);
            OLED_Set_Pos(x, y + 1);
            for (uint8_t i = 0; i < 8; i++)
                OLED_WR_Byte_Inverse(F8X16[c * 16 + i + 8], OLED_DATA);
        } else {
            OLED_Set_Pos(x, y);
            for (uint8_t i = 0; i < 6; i++)
                OLED_WR_Byte_Inverse(F6x8[c][i], OLED_DATA);
        }
    } else { // 正常显示
        if (Char_Size == OLED_SIZE_8x16) {
            OLED_Set_Pos(x, y);
            for (uint8_t i = 0; i < 8; i++)
                OLED_WR_Byte(F8X16[c * 16 + i], OLED_DATA);
            OLED_Set_Pos(x, y + 1);
            for (uint8_t i = 0; i < 8; i++)
                OLED_WR_Byte(F8X16[c * 16 + i + 8], OLED_DATA);
        } else {
            OLED_Set_Pos(x, y);
            for (uint8_t i = 0; i < 6; i++)
                OLED_WR_Byte(F6x8[c][i], OLED_DATA);
        }
    }
}

// 幂运算（内部函数）
static uint32_t oled_pow(uint8_t m, uint8_t n)
{
    uint32_t result = 1;
    while (n--) result *= m;
    return result;
}

// 显示整数
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t mode)
{
    uint8_t t, temp, enshow = 0;
    for (t = 0; t < len; t++) {
        temp = (num / oled_pow(10, len - t - 1)) % 10;
        if (enshow == 0 && t < (len - 1)) {
            if (temp == 0) {
                OLED_ShowChar(x + (size / 2) * t, y, ' ', size, mode);
                continue;
            } else {
                enshow = 1;
            }
        }
        OLED_ShowChar(x + (size / 2) * t, y, temp + '0', size, mode);
    }
}

// 显示字符串
void OLED_ShowString(uint8_t x, uint8_t y, const char *chr, uint8_t Char_Size, uint8_t mode)
{
    uint8_t j = 0;
    while (chr[j] != '\0') {
        OLED_ShowChar(x, y, chr[j], Char_Size, mode);
        x += Char_Size / 2;
        if (x > 120) {
            x = 0;
            y += 2;
        }
        j++;
    }
}

// 显示汉字（使用字库）
void OLED_ShowCHinese(uint8_t x, uint8_t y, const char *s, uint8_t mode)
{
    uint16_t HZnum = sizeof(tfont16) / sizeof(typFNT_GB16);
    for (uint16_t k = 0; k < HZnum; k++) {
        if ((tfont16[k].Index[0] == *(s)) &&
            (tfont16[k].Index[1] == *(s + 1)) &&
            (tfont16[k].Index[2] == *(s + 2))) {
            OLED_Set_Pos(x, y);
            for (uint8_t i = 0; i < 16; i++) {
                if (mode == OLED_MODE_INVERSE)
                    OLED_WR_Byte_Inverse(tfont16[k].Msk[i], OLED_DATA);
                else
                    OLED_WR_Byte(tfont16[k].Msk[i], OLED_DATA);
            }
            OLED_Set_Pos(x, y + 1);
            for (uint8_t i = 16; i < 32; i++) {
                if (mode == OLED_MODE_INVERSE)
                    OLED_WR_Byte_Inverse(tfont16[k].Msk[i], OLED_DATA);
                else
                    OLED_WR_Byte(tfont16[k].Msk[i], OLED_DATA);
            }
            break;
        }
    }
}

// 显示中英文混合文本
void OLED_ShowText(uint8_t x, uint8_t y, const char *text, uint8_t Char_Size, uint8_t mode)
{
    while (*text) {
        if ((*text & 0xE0) == 0xE0) { // 三字节 UTF-8 头
            OLED_ShowCHinese(x, y, text, mode);
            text += 3;
            x += 16;
        } else {
            OLED_ShowChar(x, y, *text, Char_Size, mode);
            text += 1;
            x += 8;
        }
    }
}

// 显示 BMP 图片
void OLED_DrawBMP(uint8_t x0, uint8_t x1, uint8_t page0, uint8_t page1, const uint8_t BMP[])
{
    uint32_t i = 0;
    for (uint8_t page = page0; page < page1; page++) {
        OLED_Set_Pos(x0, page);
        for (uint8_t x = x0; x < x1; x++) {
            OLED_WR_Byte(BMP[i++], OLED_DATA);
        }
    }
}

// OLED 初始化
void OLED_Init(void)
{
    // 配置 MyI2C 设备参数（需根据实际硬件修改引脚和延迟）
    g_oled_i2c_dev.scl_port = OLED_SCL_GPIO_Port;
    g_oled_i2c_dev.scl_pin  = OLED_SCL_Pin;
    g_oled_i2c_dev.sda_port = OLED_SDA_GPIO_Port;
    g_oled_i2c_dev.sda_pin  = OLED_SDA_Pin;
    g_oled_i2c_dev.delay_us = 0;
    g_oled_i2c_dev.addr     = OLED_ADDR;

    MyI2C_Init(&g_oled_i2c_dev);

    uint8_t test_cmd = 0xAE;
    uint8_t ret      = MyI2C_Write(&g_oled_i2c_dev, &test_cmd, 1);
    if (ret == 0) {
        // 通信失败：可以闪烁 LED 或死循环
        while (1) {
            HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
            HAL_Delay(500);
        }
    }

    // 0.96寸 SSD1306 初始化序列
    OLED_WR_Byte(0xAE, OLED_CMD);
    OLED_WR_Byte(0xD5, OLED_CMD);
    OLED_WR_Byte(0x80, OLED_CMD);
    OLED_WR_Byte(0xA8, OLED_CMD);
    OLED_WR_Byte(0x3F, OLED_CMD);
    OLED_WR_Byte(0xD3, OLED_CMD);
    OLED_WR_Byte(0x00, OLED_CMD);
    OLED_WR_Byte(0xA1, OLED_CMD);
    OLED_WR_Byte(0xC8, OLED_CMD);
    OLED_WR_Byte(0xDA, OLED_CMD);
    OLED_WR_Byte(0x12, OLED_CMD);
    OLED_WR_Byte(0x81, OLED_CMD);
    OLED_WR_Byte(0xCF, OLED_CMD);
    OLED_WR_Byte(0xD9, OLED_CMD);
    OLED_WR_Byte(0xF1, OLED_CMD);
    OLED_WR_Byte(0xDB, OLED_CMD);
    OLED_WR_Byte(0x30, OLED_CMD);
    OLED_WR_Byte(0xA4, OLED_CMD);
    OLED_WR_Byte(0xA6, OLED_CMD);
    OLED_WR_Byte(0x8D, OLED_CMD);
    OLED_WR_Byte(0x14, OLED_CMD);
    OLED_WR_Byte(0xAF, OLED_CMD);

    OLED_Clear();
}