#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* DHT11 Variables */
uint8_t Rh_byte1, Rh_byte2, Temp_byte1, Temp_byte2;
uint16_t SUM;
uint8_t Temp, Hum;

/* Function Prototypes */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);

/* Microsecond delay using DWT */
void delay_us(uint16_t us)
{
	uint32_t start = DWT->CYCCNT;
	uint32_t ticks = us * (SystemCoreClock / 1000000);
	while ((DWT->CYCCNT - start) < ticks);
}

/* Set PA1 as input */
void Set_Pin_Input(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = GPIO_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}

/* Set PA1 as output */
void Set_Pin_Output(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = GPIO_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}

/* Start signal */
void DHT11_Start(void)
{
	Set_Pin_Output(GPIOA, GPIO_PIN_1);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
	HAL_Delay(18);

	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
	delay_us(30);

	Set_Pin_Input(GPIOA, GPIO_PIN_1);
}

/* Check sensor response */
uint8_t DHT11_Check_Response(void)
{
	uint8_t Response = 0;

	delay_us(40);

	if(!(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1)))
	{
		delay_us(80);

		if((HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1)))
			Response = 1;
		else
			Response = 0;
	}

	while(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1));

	return Response;
}

/* Read 8 bits */
uint8_t DHT11_Read(void)
{
	uint8_t i=0,j;

	for(j=0;j<8;j++)
	{
		while(!(HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_1)));

		delay_us(40);

		if(!(HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_1)))
			i &= ~(1<<(7-j));
		else
			i |= (1<<(7-j));

		while(HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_1));
	}
	return i;
}

int main(void)
{

	HAL_Init();
	SystemClock_Config();

	MX_GPIO_Init();
	MX_USART2_UART_Init();

	/* Enable DWT counter for microsecond delay */
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CYCCNT = 0;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

	char msg[60];

	while (1)
	{
		DHT11_Start();
		DHT11_Check_Response();

		Rh_byte1 = DHT11_Read();
		Rh_byte2 = DHT11_Read();
		Temp_byte1 = DHT11_Read();
		Temp_byte2 = DHT11_Read();
		SUM = DHT11_Read();

		Temp = Temp_byte1;
		Hum = Rh_byte1;

		sprintf(msg,"Temp:%d C  Hum:%d %%\r\n",Temp,Hum);
		HAL_UART_Transmit(&huart2,(uint8_t*)msg,strlen(msg),100);

		HAL_Delay(2000);
	}
}

/* USART2 Init */
static void MX_USART2_UART_Init(void)
{

	huart2.Instance = USART2;
	huart2.Init.BaudRate = 115200;
	huart2.Init.WordLength = UART_WORDLENGTH_8B;
	huart2.Init.StopBits = UART_STOPBITS_1;
	huart2.Init.Parity = UART_PARITY_NONE;
	huart2.Init.Mode = UART_MODE_TX_RX;
	huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart2.Init.OverSampling = UART_OVERSAMPLING_16;

	if (HAL_UART_Init(&huart2) != HAL_OK)
	{
		Error_Handler();
	}
}

/* GPIO Init */
static void MX_GPIO_Init(void)
{

	GPIO_InitTypeDef GPIO_InitStruct = {0};

	__HAL_RCC_GPIOA_CLK_ENABLE();

	GPIO_InitStruct.Pin = GPIO_PIN_1;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/* Clock Configuration */
void SystemClock_Config(void)
{

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

	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
			|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;

	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}

void Error_Handler(void)
{
	while(1){}
}
