//DHT11->STM32->LCD DISPLAY
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* LCD Address - Try 0x4E (0x27<<1) or 0x7E (0x3F<<1) */
#define LCD_ADDR (0x27 << 1)

/* Private variables ---------------------------------------------------------*/
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

/* GPIO helper functions */
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

/* LCD Functions */
void lcd_send_cmd(char cmd) {
    uint8_t data_u, data_l;
    uint8_t data_t[4];
    data_u = (cmd & 0xf0);
    data_l = ((cmd << 4) & 0xf0);
    data_t[0] = data_u | 0x0C;  //en=1, rs=0
    data_t[1] = data_u | 0x08;  //en=0, rs=0
    data_t[2] = data_l | 0x0C;  //en=1, rs=0
    data_t[3] = data_l | 0x08;  //en=0, rs=0
    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, (uint8_t *)data_t, 4, 100);
}

void lcd_send_data(char data) {
    uint8_t data_u, data_l;
    uint8_t data_t[4];
    data_u = (data & 0xf0);
    data_l = ((data << 4) & 0xf0);
    data_t[0] = data_u | 0x0D;  //en=1, rs=1
    data_t[1] = data_u | 0x09;  //en=0, rs=1
    data_t[2] = data_l | 0x0D;  //en=1, rs=1
    data_t[3] = data_l | 0x09;  //en=0, rs=1
    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, (uint8_t *)data_t, 4, 100);
}

void lcd_init(void) {
    HAL_Delay(50);
    lcd_send_cmd(0x30); HAL_Delay(5);
    lcd_send_cmd(0x30); HAL_Delay(1);
    lcd_send_cmd(0x30); HAL_Delay(10);
    lcd_send_cmd(0x20); HAL_Delay(10);
    lcd_send_cmd(0x28); HAL_Delay(1);
    lcd_send_cmd(0x08); HAL_Delay(1);
    lcd_send_cmd(0x01); HAL_Delay(2);
    lcd_send_cmd(0x06); HAL_Delay(1);
    lcd_send_cmd(0x0C);
}

void lcd_send_string(char *str) {
    while (*str) lcd_send_data(*str++);
}

void lcd_put_cur(int row, int col) {
    switch (row) {
        case 0: col |= 0x80; break;
        case 1: col |= 0xC0; break;
    }
    lcd_send_cmd(col);
}

/* DHT11 Functions */
void DHT11_Start(void) {
    Set_Pin_Output(GPIOA, GPIO_PIN_1);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, 0);
    HAL_Delay(18);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, 1);
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
    while(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) && timeout < 5000) timeout++;
    return Response;
}

uint8_t DHT11_Read(void) {
    uint8_t i, j;
    for (j = 0; j < 8; j++) {
        while (!(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1)));
        delay_us(40);
        if (!(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1)))
            i &= ~(1 << (7 - j));
        else {
            i |= (1 << (7 - j));
            while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1));
        }
    }
    return i;
}

/* Main Program */
int main(void) {
    HAL_Init();
    SystemClock_Config();

    /* Enable DWT for us delay */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_USART2_UART_Init();

    lcd_init();
    lcd_put_cur(0, 0);
    lcd_send_string("DHT11 Loading...");

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

                sprintf(msg, "Temp: %d C    ", Temp);
                lcd_put_cur(0, 0);
                lcd_send_string(msg);

                sprintf(msg, "Hum:  %d %%    ", Hum);
                lcd_put_cur(1, 0);
                lcd_send_string(msg);

                sprintf(msg, "T:%d H:%d\r\n", Temp, Hum);
                HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);
            }
        } else {
            lcd_put_cur(0, 0);
            lcd_send_string("Sensor Error   ");
        }
        HAL_Delay(2000);
    }
}

/* PERIPHERAL DEFINITIONS */
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
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
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&hi2c1);
}

static void MX_USART2_UART_Init(void) {
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);
}

static void MX_GPIO_Init(void) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* I2C1 Pins: PB6 -> SCL, PB7 -> SDA */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void Error_Handler(void) {
    while(1);
}
