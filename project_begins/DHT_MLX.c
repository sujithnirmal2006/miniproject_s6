//here there is only main.c the other file follows the previous option which mentioned in DHT_OLED
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "ssd1306.h"
#include "fonts.h"

/* MLX90614 Definitions */
#define MLX90614_ADDR (0x5A << 1)
#define MLX90614_RAM_ACCESS 0x00
#define MLX90614_OBJ1 0x07  // Register for Object Temperature

/* Private variables */
I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart2;

uint8_t Rh_byte1, Rh_byte2, Temp_byte1, Temp_byte2;
uint16_t SUM;
uint8_t Temp, Hum;
float bodyTemp = 0;

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

/* MLX90614 Read Function */
/* Put this at the bottom of main.c or replace the existing one */
float MLX90614_ReadTemp(uint8_t reg) {
    uint8_t data[3];
    HAL_StatusTypeDef status;

    // Attempt to read 3 bytes from the MLX90614
    status = HAL_I2C_Mem_Read(&hi2c1, MLX90614_ADDR, reg, I2C_MEMADD_SIZE_8BIT, data, 3, 500);

    if (status != HAL_OK) {
        // ERROR: The sensor didn't answer!
        // We return -1.0 so we can see the error on the screen/serial
        return -1.0f;
    }

    // Combine MSB and LSB
    uint16_t raw = (data[1] << 8) | data[0];

    // Formula from the MLX90614 datasheet
    return (raw * 0.02f) - 273.15f;
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
            // --- 1. Read DHT11 (Same as before) ---
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
                }
            }

            // --- 2. Read MLX90614 ---
            bodyTemp = MLX90614_ReadTemp(MLX90614_OBJ1);

            // --- 3. THE DECIMAL TRICK ---
            // If bodyTemp is 33.45:
            int wholePart = (int)bodyTemp;                    // Result: 33
            int decimalPart = (int)((bodyTemp - wholePart) * 10); // Result: 4
            // To handle negative rounding, make sure decimal is positive
            if(decimalPart < 0) decimalPart *= -1;

            // --- 4. Update OLED Display ---
            ssd1306_Clear();
            sprintf(msg, "ROOM: %dC %d%%", Temp, Hum);
            ssd1306_SetCursor(0, 0);
            ssd1306_WriteString(msg, Font_5x7);

            // We print "33" then a "." then "4" manually
            sprintf(msg, "BODY: %d.%d C", wholePart, decimalPart);
            ssd1306_SetCursor(0, 2);
            ssd1306_WriteString(msg, Font_5x7);
            ssd1306_UpdateScreen();

            // --- 5. Serial Output ---
            sprintf(msg, "Room:%d Hum:%d Body:%d.%d\r\n", Temp, Hum, wholePart, decimalPart);
            HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);

            HAL_Delay(1000);
        }
}

/* Hardware Init Definitions (unchanged) */
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
