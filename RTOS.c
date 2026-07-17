/* =============================================================================
 * main.c — STM32F4 Health Monitor
 *
 * Sensors    : MAX30105 (IR/SpO2)  → I2C1 (PB6/PB7, addr 0x57)
 *              MLX90614 (Body Temp)→ I2C1 (PB6/PB7, addr 0x5A)
 *              DHT11    (Room Env) → PA1  (bit-bang GPIO)
 * Display    : SSD1306 OLED 128x64 → I2C1 (PB6/PB7, addr 0x3C)
 * UART1      : PA9(TX)/PA10(RX)   → ESP32 / Blynk  (115200 baud)
 * UART2      : PA2(TX)/PA3(RX)    → PC debug        (115200 baud)
 *
 * Dashboard layout (mirrors Arduino sketch):
 *   ┌─────────────────────────────┐
 *   │ ENV: 28C | 65%H             │  ← header row
 *   ├─────────────────────────────┤
 *   │ BODY TEMPERATURE            │
 *   │        36.8 C               │  ← big text
 *   ├──────────────┬──────────────┤
 *   │ BPM          │ SpO2         │
 *   │  74          │  98          │
 *   └──────────────┴──────────────┘
 *
 * Data format sent via UART1 / UART2 (CSV):
 *   IR,BPM,SpO2,BodyTemp_int,BodyTemp_dec,RoomTemp,Humidity\r\n
 * =============================================================================
 */

#include "main.h"
#include "ssd1306.h"
#include "fonts.h"
#include "max30100.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>   /* rand(), srand() */
#include <stdint.h>

/* ── Peripheral handles ───────────────────────────────────────── */
I2C_HandleTypeDef  hi2c1;
UART_HandleTypeDef huart1;   /* ESP32 / Blynk  */
UART_HandleTypeDef huart2;   /* PC debug       */
// ADC_HandleTypeDef  hadc1;    /* unused here but kept for linker */

/* ── MLX90614 ─────────────────────────────────────────────────── */
#define MLX90614_ADDR   (0x5A << 1)
#define MLX90614_OBJ1   0x07

/* ── Finger-detect threshold (matches Arduino sketch) ─────────── */
#define FINGER_THRESHOLD  45000UL

/* ── BPM / SpO2 simulation ranges (matching Arduino random()) ─── */
#define BPM_MIN   72
#define BPM_MAX   76
#define SPO2_MIN  97
#define SPO2_MAX 100

/* ── Body-temp calibration offset (same as Arduino +2.4) ──────── */
#define BODY_TEMP_OFFSET  2.4f

/* ── DHT11 on PA1 ─────────────────────────────────────────────── */
#define DHT_PORT  GPIOA
#define DHT_PIN   GPIO_PIN_1

/* ── Timing ───────────────────────────────────────────────────── */
#define DHT_READ_INTERVAL_MS   2000u
#define UART_SEND_INTERVAL_MS   200u
#define DISPLAY_INTERVAL_MS     500u

/* ── Function prototypes ──────────────────────────────────────── */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
void Error_Handler(void);

/* DWT µs delay */
static void delay_us(uint16_t us);

/* DHT11 helpers */
static void     DHT11_SetOutput(void);
static void     DHT11_SetInput(void);
static uint8_t  DHT11_Read(void);
static uint8_t  DHT11_Acquire(uint8_t *temp, uint8_t *hum);

/* MLX90614 */
static float MLX90614_ReadTemp(uint8_t reg);

/* Simulation helpers */
static int  sim_range(int lo, int hi);   /* inclusive random int */

/* ============================================================== */
/*  MAIN                                                           */
/* ============================================================== */
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    /* Enable DWT cycle counter for delay_us() */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;

    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();

    /* Seed simulator with a cycle-counter snapshot */
    srand((unsigned)DWT->CYCCNT);

    /* ── Init peripherals ──────────────────────────────────────── */
    ssd1306_Init();
    MAX30105_Init();

    /* Splash screen (matches Arduino "HARDWARE SYNC...") */
    ssd1306_Clear();
    ssd1306_SetCursor(10, 3);
    ssd1306_WriteString("HARDWARE SYNC...", Font_5x7);
    ssd1306_UpdateScreen();
    HAL_Delay(2000);

    /* ── Working variables ─────────────────────────────────────── */
    uint8_t  roomTemp = 0, humidity = 0;
    uint8_t  dhtOk    = 0;
    float    bodyTemp = 0.0f;
    uint32_t irValue  = 0;
    int      bpm = 0, spo2 = 0;

    uint32_t lastDHT     = 0;
    uint32_t lastUART    = 0;
    uint32_t lastDisplay = 0;

    char lineBuf[64];

    /* ── Main loop ─────────────────────────────────────────────── */
    while (1)
    {
        uint32_t now = HAL_GetTick();

        /* 1. DHT11 — every 2 s ----------------------------------- */
        if (now - lastDHT >= DHT_READ_INTERVAL_MS)
        {
            dhtOk    = DHT11_Acquire(&roomTemp, &humidity);
            lastDHT  = now;
        }

        /* 2. MLX90614 body temperature (fast I2C) --------------- */
        bodyTemp = MLX90614_ReadTemp(MLX90614_OBJ1);
        if (bodyTemp > 0.0f)
            bodyTemp += BODY_TEMP_OFFSET;   /* calibration offset  */

        /* 3. MAX30105 IR value ----------------------------------- */
        irValue = MAX30105_ReadIR();

        /* 4. Simulate BPM & SpO2 when finger detected ------------ */
        if (irValue > FINGER_THRESHOLD)
        {
            bpm  = sim_range(BPM_MIN,  BPM_MAX);
            spo2 = sim_range(SPO2_MIN, SPO2_MAX);
        }
        else
        {
            bpm  = 0;
            spo2 = 0;
        }

        /* 5. OLED dashboard — every 500 ms ----------------------- */
        if (now - lastDisplay >= DISPLAY_INTERVAL_MS)
        {
            /* Integer and one-decimal for body temp */
            int bodyInt = (int)bodyTemp;
            int bodyDec = (bodyTemp > 0.0f)
                          ? (int)((bodyTemp - (float)bodyInt) * 10.0f)
                          : 0;

            ssd1306_Clear();

            /* ── Outer border ─────────────────────────────────── */
            /* Drawn with thin horizontal/vertical lines in the buffer.
             * Because ssd1306 uses page addressing (8px rows) we draw
             * the header separator and mid-dividers as full-width lines
             * at page boundaries.                                      */

            /* Header row: page 0 (pixels 0-7) */
            ssd1306_SetCursor(2, 0);
            if (!dhtOk) {
                ssd1306_WriteString("ENV: DHT ERROR", Font_5x7);
            } else {
                sprintf(lineBuf, "ENV:%dC|%d%%H", roomTemp, humidity);
                ssd1306_WriteString(lineBuf, Font_5x7);
            }

            /* "BODY TEMPERATURE" label: page 2 */
            ssd1306_SetCursor(2, 2);
            ssd1306_WriteString("BODY TEMPERATURE", Font_5x7);

            /* Big body temp value: page 3 */
            ssd1306_SetCursor(28, 3);
            if (bodyTemp <= 0.0f) {
                ssd1306_WriteString("--.- C", Font_5x7);
            } else {
                sprintf(lineBuf, "%d.%d C", bodyInt, bodyDec);
                ssd1306_WriteString(lineBuf, Font_5x7);
            }

            /* Bottom section — finger present or not */
            if (irValue > FINGER_THRESHOLD)
            {
                /* BPM (left) */
                ssd1306_SetCursor(2, 5);
                ssd1306_WriteString("BPM", Font_5x7);
                ssd1306_SetCursor(6, 6);
                sprintf(lineBuf, "%d", bpm);
                ssd1306_WriteString(lineBuf, Font_5x7);

                /* SpO2 (right) */
                ssd1306_SetCursor(66, 5);
                ssd1306_WriteString("SpO2", Font_5x7);
                ssd1306_SetCursor(72, 6);
                sprintf(lineBuf, "%d%%", spo2);
                ssd1306_WriteString(lineBuf, Font_5x7);
            }
            else
            {
                ssd1306_SetCursor(10, 6);
                ssd1306_WriteString("PLACE FINGER...", Font_5x7);
            }

            ssd1306_UpdateScreen();
            lastDisplay = now;
        }

        /* 6. UART transmit — every 200 ms ------------------------ */
        if (now - lastUART >= UART_SEND_INTERVAL_MS)
        {
            int bodyInt = (int)bodyTemp;
            int bodyDec = (bodyTemp > 0.0f)
                          ? (int)((bodyTemp - (float)bodyInt) * 10.0f)
                          : 0;

            /* CSV: IR,BPM,SpO2,BodyInt,BodyDec,RoomTemp,Humidity */
            sprintf(lineBuf, "%lu,%d,%d,%d.%d,%d,%d\r\n",
                    (unsigned long)irValue, bpm, spo2,
                    bodyInt, bodyDec,
                    (int)roomTemp, (int)humidity);

            HAL_UART_Transmit(&huart1, (uint8_t *)lineBuf,
                              strlen(lineBuf), 100);   /* → ESP32  */

            char dbg[80];
            sprintf(dbg, "DBG: %s", lineBuf);
            HAL_UART_Transmit(&huart2, (uint8_t *)dbg,
                              strlen(dbg), 100);        /* → PC     */

            lastUART = now;
        }

        HAL_Delay(10);
    }
}

/* ============================================================== */
/*  SIMULATION HELPER                                              */
/* ============================================================== */

/**
 * @brief Returns a pseudo-random integer in [lo, hi] (inclusive).
 *        Mirrors Arduino random(lo, hi) which returns [lo, hi-1], but
 *        the Arduino sketch calls random(72,76) meaning 72-75 for BPM
 *        and random(97,100) meaning 97-99 for SpO2.  We match that.
 */
static int sim_range(int lo, int hi)
{
    /* Arduino random(lo, hi) is exclusive of hi */
    return lo + (rand() % (hi - lo));
}

/* ============================================================== */
/*  DWT µs DELAY                                                   */
/* ============================================================== */
static void delay_us(uint16_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = (uint32_t)us * (SystemCoreClock / 1000000UL);
    while ((DWT->CYCCNT - start) < ticks);
}

/* ============================================================== */
/*  DHT11                                                          */
/* ============================================================== */
static void DHT11_SetOutput(void)
{
    GPIO_InitTypeDef g = {0};
    g.Pin   = DHT_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DHT_PORT, &g);
}

static void DHT11_SetInput(void)
{
    GPIO_InitTypeDef g = {0};
    g.Pin  = DHT_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DHT_PORT, &g);
}

/**
 * @brief Reads one byte (8 bits) from DHT11 after the start sequence.
 */
static uint8_t DHT11_Read(void)
{
    uint8_t val = 0;
    for (int j = 7; j >= 0; j--)
    {
        /* Wait for rising edge (bit start) */
        uint32_t t = 0;
        while (!HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN) && t < 10000) t++;

        delay_us(40);   /* sample after 40 µs: 0 → pin LOW, 1 → pin HIGH */

        if (HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN))
            val |= (1 << j);

        /* Wait for line to go low (bit end) */
        t = 0;
        while (HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN) && t < 10000) t++;
    }
    return val;
}

/**
 * @brief Full DHT11 acquisition: start pulse → response check → 5 bytes.
 * @param temp  Pointer to store integer temperature (°C)
 * @param hum   Pointer to store integer humidity (%)
 * @return 1 on success, 0 on checksum/timeout failure
 */
static uint8_t DHT11_Acquire(uint8_t *temp, uint8_t *hum)
{
    /* Start pulse: pull low ≥18 ms, then release */
    DHT11_SetOutput();
    HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_RESET);
    HAL_Delay(18);
    HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_SET);
    delay_us(30);
    DHT11_SetInput();

    /* Check response: DHT pulls low ~80 µs, then high ~80 µs */
    delay_us(40);
    if (HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN)) return 0;   /* no response */
    delay_us(80);
    if (!HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN)) return 0;  /* stuck low   */

    uint32_t t = 0;
    while (HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN) && t < 10000) t++;

    /* Read 5 bytes */
    uint8_t rh1  = DHT11_Read();
    uint8_t rh2  = DHT11_Read();
    uint8_t tmp1 = DHT11_Read();
    uint8_t tmp2 = DHT11_Read();
    uint8_t csum = DHT11_Read();

    if (csum != (uint8_t)(rh1 + rh2 + tmp1 + tmp2)) return 0;

    *hum  = rh1;
    *temp = tmp1;
    return 1;
}

/* ============================================================== */
/*  MLX90614                                                       */
/* ============================================================== */
static float MLX90614_ReadTemp(uint8_t reg)
{
    uint8_t data[3];
    if (HAL_I2C_Mem_Read(&hi2c1, MLX90614_ADDR, reg,
                          I2C_MEMADD_SIZE_8BIT, data, 3, 500) != HAL_OK)
        return -1.0f;

    uint16_t raw = ((uint16_t)data[1] << 8) | data[0];
    return (raw * 0.02f) - 273.15f;
}

/* ============================================================== */
/*  PERIPHERAL INIT                                                */
/* ============================================================== */

/**
 * @brief System clock: HSI → PLL → 84 MHz
 *        (matches original project, Voltage Scale 3)
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState            = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState        = RCC_PLL_ON;
    osc.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM            = 16;
    osc.PLL.PLLN            = 336;
    osc.PLL.PLLP            = RCC_PLLP_DIV4;
    osc.PLL.PLLQ            = 2;
    HAL_RCC_OscConfig(&osc);

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                         RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2);
}

/* ── GPIO ───────────────────────────────────────────────────────── */
static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};

    /* UART1: PA9 TX, PA10 RX → ESP32 */
    g.Pin       = GPIO_PIN_9 | GPIO_PIN_10;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &g);

    /* UART2: PA2 TX, PA3 RX → PC */
    g.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
    g.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &g);

    /* DHT11: PA1 — starts as input with pull-up */
    g.Pin       = DHT_PIN;
    g.Mode      = GPIO_MODE_INPUT;
    g.Pull      = GPIO_PULLUP;
    g.Alternate = 0;
    HAL_GPIO_Init(DHT_PORT, &g);

    /* I2C1: PB6 SCL, PB7 SDA — open-drain + pull-up (from HAL_MSP) */
    /* Configured inside HAL_I2C_MspInit in stm32f4xx_hal_msp.c      */
}

/* ── I2C1 ────────────────────────────────────────────────────────── */
static void MX_I2C1_Init(void)
{
    hi2c1.Instance             = I2C1;
    hi2c1.Init.ClockSpeed      = 100000;
    hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1     = 0;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&hi2c1);
}

/* ── USART1 (ESP32) ──────────────────────────────────────────────── */
static void MX_USART1_UART_Init(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();

    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1);
}

/* ── USART2 (PC debug) ───────────────────────────────────────────── */
static void MX_USART2_UART_Init(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();

    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 115200;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);
}

/* ── Error handler ───────────────────────────────────────────────── */
void Error_Handler(void)
{
    __disable_irq();
    while (1);
}
