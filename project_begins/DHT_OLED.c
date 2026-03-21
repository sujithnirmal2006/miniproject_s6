//main.c
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "ssd1306.h"
#include "fonts.h"

/* Private variables */
I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart2;

uint8_t Rh_byte1, Rh_byte2, Temp_byte1, Temp_byte2;
uint16_t SUM;
uint8_t Temp, Hum;

/* Function Prototypes */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART2_UART_Init(void);

/* Microsecond delay using DWT */
void delay_us(uint16_t us) {
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000);
    while ((DWT->CYCCNT - start) < ticks);
}

/* GPIO Helper Functions */
void Set_Pin_Output(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}

void Set_Pin_Input(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}

/* DHT11 Implementation */
void DHT11_Start(void) {
    Set_Pin_Output(GPIOA, GPIO_PIN_1);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
    HAL_Delay(18);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
    delay_us(30);
    Set_Pin_Input(GPIOA, GPIO_PIN_1);
}

uint8_t DHT11_Check_Response(void) {
    uint8_t Response = 0;
    delay_us(40);
    if (!(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1))) {
        delay_us(80);
        if ((HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1))) Response = 1;
    }
    uint32_t timeout = 0;
    while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) && timeout < 1000) {
        timeout++; delay_us(1);
    }
    return Response;
}

uint8_t DHT11_Read(void) {
    uint8_t i = 0, j;
    for (j = 0; j < 8; j++) {
        uint32_t timeout = 0;
        while (!(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1)) && timeout < 1000) {
            timeout++; delay_us(1);
        }
        delay_us(40);
        if (!(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1))) {
            i &= ~(1 << (7 - j));
        } else {
            i |= (1 << (7 - j));
            timeout = 0;
            while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) && timeout < 1000) {
                timeout++; delay_us(1);
            }
        }
    }
    return i;
}

int main(void) {
    HAL_Init();
    SystemClock_Config();

    /* Enable DWT */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_USART2_UART_Init();

    ssd1306_Init();
    char msg[32];

    while (1) {
        DHT11_Start();
        if (DHT11_Check_Response()) {
            Rh_byte1 = DHT11_Read();
            Rh_byte2 = DHT11_Read();
            Temp_byte1 = DHT11_Read();
            Temp_byte2 = DHT11_Read();
            SUM = DHT11_Read();

            if (SUM == (uint8_t)(Rh_byte1 + Rh_byte2 + Temp_byte1 + Temp_byte2)) {
                Temp = Temp_byte1;
                Hum = Rh_byte1;

                ssd1306_Clear();
                sprintf(msg, "TEMP: %d C", Temp);
                ssd1306_SetCursor(0, 0);
                ssd1306_WriteString(msg, Font_5x7);

                sprintf(msg, "HUM : %d %%", Hum);
                ssd1306_SetCursor(0, 2);
                ssd1306_WriteString(msg, Font_5x7);
                ssd1306_UpdateScreen();

                sprintf(msg, "TEMP:%d HUM:%d\r\n", Temp, Hum);
                HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);
            }
        } else {
            ssd1306_Clear();
            ssd1306_SetCursor(0, 0);
            ssd1306_WriteString("Sensor Error", Font_5x7);
            ssd1306_UpdateScreen();
        }
        HAL_Delay(2000);
    }
}

/* Hardware Init Definitions */
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 16;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
    RCC_OscInitStruct.PLL.PLLQ = 2;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}

static void MX_I2C1_Init(void) {
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 100000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    HAL_I2C_Init(&hi2c1);
}

static void MX_USART2_UART_Init(void) {
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    HAL_UART_Init(&huart2);
}

static void MX_GPIO_Init(void) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}
//SSD1306.C
#include "ssd1306.h"
#include <string.h>

extern I2C_HandleTypeDef hi2c1;

// Screen buffer: 128 columns * 8 pages (64 rows / 8) = 1024 bytes
static uint8_t buffer[1024];
static uint8_t cursor_x = 0;
static uint8_t cursor_y = 0;

/**
 * @brief Sends a command to the SSD1306 controller
 */
void ssd1306_WriteCommand(uint8_t cmd) {
    uint8_t data[2] = {0x00, cmd};
    HAL_I2C_Master_Transmit(&hi2c1, SSD1306_I2C_ADDR << 1, data, 2, 10);
}

/**
 * @brief Initializes the OLED display with standard 128x64 settings
 */
void ssd1306_Init(void) {
    HAL_Delay(100); // Wait for screen to power up

    ssd1306_WriteCommand(0xAE); // Display off
    ssd1306_WriteCommand(0x20); // Set Memory Addressing Mode
    ssd1306_WriteCommand(0x10); // Page addressing mode
    ssd1306_WriteCommand(0xB0); // Set Page Start Address for Page addressing mode
    ssd1306_WriteCommand(0xC8); // Set COM Output Scan Direction
    ssd1306_WriteCommand(0x00); // Set low column address
    ssd1306_WriteCommand(0x10); // Set high column address
    ssd1306_WriteCommand(0x40); // Set start line address
    ssd1306_WriteCommand(0x81); // Set contrast control register
    ssd1306_WriteCommand(0xFF);
    ssd1306_WriteCommand(0xA1); // Set segment re-map 0 to 127
    ssd1306_WriteCommand(0xA6); // Set normal display
    ssd1306_WriteCommand(0xA8); // Set multiplex ratio(1 to 64)
    ssd1306_WriteCommand(0x3F);
    ssd1306_WriteCommand(0xA4); // 0xa4,Output follows RAM content;0xa5,Output ignores RAM content
    ssd1306_WriteCommand(0xD3); // Set display offset
    ssd1306_WriteCommand(0x00); // No offset
    ssd1306_WriteCommand(0xD5); // Set display clock divide ratio/oscillator frequency
    ssd1306_WriteCommand(0xF0); // Set divide ratio
    ssd1306_WriteCommand(0xD9); // Set pre-charge period
    ssd1306_WriteCommand(0x22);
    ssd1306_WriteCommand(0xDA); // Set com pins hardware configuration
    ssd1306_WriteCommand(0x12);
    ssd1306_WriteCommand(0xDB); // Set vcomh
    ssd1306_WriteCommand(0x20); // 0.77xVcc
    ssd1306_WriteCommand(0x8D); // Set DC-DC enable
    ssd1306_WriteCommand(0x14);
    ssd1306_WriteCommand(0xAF); // Display ON

    ssd1306_Clear();
    ssd1306_UpdateScreen();
}

/**
 * @brief Wipes the local buffer
 */
void ssd1306_Clear(void) {
    memset(buffer, 0, sizeof(buffer));
}

/**
 * @brief Pushes the buffer to the OLED hardware via I2C
 */
void ssd1306_UpdateScreen(void) {
    for (uint8_t i = 0; i < 8; i++) {
        ssd1306_WriteCommand(0xB0 + i); // Set page address
        ssd1306_WriteCommand(0x00);      // Set low column address
        ssd1306_WriteCommand(0x10);      // Set high column address

        uint8_t data[129];
        data[0] = 0x40; // Control byte: Data follows
        memcpy(&data[1], &buffer[i * 128], 128);

        HAL_I2C_Master_Transmit(&hi2c1, SSD1306_I2C_ADDR << 1, data, 129, 100);
    }
}

/**
 * @brief Sets the cursor position
 * @param x Horizontal position (0-127)
 * @param y Page position (0-7)
 */
void ssd1306_SetCursor(uint8_t x, uint8_t y) {
    cursor_x = x;
    cursor_y = y;
}

/**
 * @brief Writes a single character to the buffer
 */
void ssd1306_WriteChar(char ch, FontDef Font) {
    uint8_t i;

    // 1. Check if character is within ASCII range
    if (ch < 32 || ch > 126) ch = ' ';

    // 2. Prevent buffer overflow
    if (cursor_x + Font.width >= 128) return;

    // 3. Map the font columns to the buffer
    for (i = 0; i < Font.width; i++) {
        // Each byte in Font.data is a vertical slice of 8 pixels
        // We place it directly into the buffer at the current page (cursor_y)
        buffer[cursor_x + (cursor_y * 128)] = Font.data[(ch - 32) * Font.width + i];
        cursor_x++;
    }

    // 4. Add 1 pixel space between characters
    cursor_x++;
}

/**
 * @brief Writes a string to the buffer
 */
void ssd1306_WriteString(char* str, FontDef Font) {
    while (*str) {
        ssd1306_WriteChar(*str, Font);
        str++;
    }
}
//SSD1306.h
#ifndef __SSD1306_H
#define __SSD1306_H

#include "stm32f4xx_hal.h"
#include "fonts.h"

#define SSD1306_I2C_ADDR 0x3C

void ssd1306_Init(void);
void ssd1306_Clear(void);
void ssd1306_UpdateScreen(void);
void ssd1306_SetCursor(uint8_t x, uint8_t y);
void ssd1306_WriteChar(char ch, FontDef Font);
void ssd1306_WriteString(char* str, FontDef Font);

#endif
//FONTS.C
#include "fonts.h"

// 5x7 Font: Each character is 5 bytes (columns) long.
// Vertical bit mapping: LSB is at the top.
static const uint8_t Font5x7_Data[] = {
    0x00,0x00,0x00,0x00,0x00, // (space)
    0x00,0x00,0x5F,0x00,0x00, // !
    0x00,0x07,0x00,0x07,0x00, // "
    0x14,0x7F,0x14,0x7F,0x14, // #
    0x24,0x2A,0x7F,0x2A,0x12, // $
    0x23,0x13,0x08,0x64,0x62, // %
    0x36,0x49,0x55,0x22,0x50, // &
    0x00,0x05,0x03,0x00,0x00, // '
    0x00,0x1C,0x22,0x41,0x00, // (
    0x00,0x41,0x22,0x1C,0x00, // )
    0x14,0x08,0x3E,0x08,0x14, // *
    0x08,0x08,0x3E,0x08,0x08, // +
    0x00,0x50,0x30,0x00,0x00, // ,
    0x08,0x08,0x08,0x08,0x08, // -
    0x00,0x60,0x60,0x00,0x00, // .
    0x20,0x10,0x08,0x04,0x02, // /
    0x3E,0x51,0x49,0x45,0x3E, // 0
    0x00,0x42,0x7F,0x40,0x00, // 1
    0x42,0x61,0x51,0x49,0x46, // 2
    0x21,0x41,0x45,0x4B,0x31, // 3
    0x18,0x14,0x12,0x7F,0x10, // 4
    0x27,0x45,0x45,0x45,0x39, // 5
    0x3C,0x4A,0x49,0x49,0x30, // 6
    0x01,0x71,0x09,0x05,0x03, // 7
    0x36,0x49,0x49,0x49,0x36, // 8
    0x06,0x49,0x49,0x29,0x1E, // 9
    0x00,0x36,0x36,0x00,0x00, // :
    0x00,0x56,0x36,0x00,0x00, // ;
    0x08,0x14,0x22,0x41,0x00, // <
    0x14,0x14,0x14,0x14,0x14, // =
    0x00,0x41,0x22,0x14,0x08, // >
    0x02,0x01,0x51,0x09,0x06, // ?
    0x32,0x49,0x79,0x41,0x3E, // @
    0x7E,0x11,0x11,0x11,0x7E, // A
    0x7F,0x49,0x49,0x49,0x36, // B
    0x3E,0x41,0x41,0x41,0x22, // C
    0x7F,0x41,0x41,0x22,0x1C, // D
    0x7F,0x49,0x49,0x49,0x41, // E
    0x7F,0x09,0x09,0x09,0x01, // F
    0x3E,0x41,0x49,0x49,0x7A, // G
    0x7F,0x08,0x08,0x08,0x7F, // H
    0x00,0x41,0x7F,0x41,0x00, // I
    0x20,0x40,0x41,0x3F,0x01, // J
    0x7F,0x08,0x14,0x22,0x41, // K
    0x7F,0x40,0x40,0x40,0x40, // L
    0x7F,0x02,0x0C,0x02,0x7F, // M
    0x7F,0x04,0x08,0x10,0x7F, // N
    0x3E,0x41,0x41,0x41,0x3E, // O
    0x7F,0x09,0x09,0x09,0x06, // P
    0x3E,0x41,0x51,0x21,0x5E, // Q
    0x7F,0x09,0x19,0x29,0x46, // R
    0x46,0x49,0x49,0x49,0x31, // S
    0x01,0x01,0x7F,0x01,0x01, // T
    0x3F,0x40,0x40,0x40,0x3F, // U
    0x1F,0x20,0x40,0x20,0x1F, // V
    0x3F,0x40,0x38,0x40,0x3F, // W
    0x63,0x14,0x08,0x14,0x63, // X
    0x07,0x08,0x70,0x08,0x07, // Y
    0x61,0x51,0x49,0x45,0x43, // Z
    0x00,0x7F,0x41,0x41,0x00, // [
    0x02,0x04,0x08,0x10,0x20, // \
    0x00,0x41,0x41,0x7F,0x00, // ]
    0x04,0x02,0x01,0x02,0x04, // ^
    0x40,0x40,0x40,0x40,0x40, // _
    0x00,0x01,0x02,0x04,0x00, // `
    0x20,0x54,0x54,0x54,0x78, // a
    0x7F,0x48,0x44,0x44,0x38, // b
    0x38,0x44,0x44,0x44,0x20, // c
    0x38,0x44,0x44,0x48,0x7F, // d
    0x38,0x54,0x54,0x54,0x18, // e
    0x08,0x7E,0x09,0x01,0x02, // f
    0x0C,0x52,0x52,0x52,0x3E, // g
    0x7F,0x08,0x04,0x04,0x78, // h
    0x00,0x44,0x7D,0x40,0x00, // i
    0x20,0x40,0x44,0x3D,0x00, // j
    0x7F,0x10,0x28,0x44,0x00, // k
    0x00,0x41,0x7F,0x40,0x00, // l
    0x7C,0x04,0x18,0x04,0x78, // m
    0x7C,0x08,0x04,0x04,0x78, // n
    0x38,0x44,0x44,0x44,0x38, // o
    0x7C,0x14,0x14,0x14,0x08, // p
    0x08,0x14,0x14,0x18,0x7C, // q
    0x7C,0x08,0x04,0x04,0x00, // r
    0x48,0x54,0x54,0x54,0x20, // s
    0x04,0x3F,0x44,0x40,0x20, // t
    0x3C,0x40,0x40,0x20,0x7C, // u
    0x1C,0x20,0x40,0x20,0x1C, // v
    0x3C,0x40,0x30,0x40,0x3C, // w
    0x44,0x28,0x10,0x28,0x44, // x
    0x0C,0x50,0x50,0x50,0x3C, // y
    0x44,0x64,0x54,0x4C,0x44  // z
};

FontDef Font_5x7 = {5, 7, Font5x7_Data};
//FONTS.h
#ifndef __FONTS_H
#define __FONTS_H

#include <stdint.h>

typedef struct {
    const uint8_t width;
    const uint8_t height;
    const uint8_t *data;
} FontDef;

extern FontDef Font_5x7;

#endif



