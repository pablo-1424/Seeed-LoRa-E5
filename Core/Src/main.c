/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "app_lorawan.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "string.h"
#include "stm32_timer.h"
#include "lora_app.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MESSAGE_WAKE_UP 0
#define MESSAGE_START 1
#define MESSAGE_END 2

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint8_t rxPointer = 0;
char rxBuffer[256] = {'\0'};

extern IRDA_HandleTypeDef hirda1;

static UTIL_TIMER_Object_t wakeUpSendTimer;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
uint8_t IRDA_Receive(IRDA_HandleTypeDef hirda) {
	uint8_t reception[1];

	// Wait for 10 ms to see if anything else has been sent
	switch (HAL_IRDA_Receive(&hirda, reception, sizeof(reception), 10)) {
		case HAL_OK: {
			rxBuffer[rxPointer++] = reception[0];
			break;
		};
		case HAL_BUSY: {
			HAL_Delay(5);
			break;
		};
		case HAL_ERROR: {
			break;
		};
		// Reception is complete
		case HAL_TIMEOUT: {
			return 1;
		};
	}

	return 0;
}

// Send "A\r"
void IRDA_Transmit_Wake_Up(void) {
	uint8_t messageSend[] = "A\r";

	HAL_IRDA_Transmit(&hirda1, messageSend, sizeof(messageSend) - 1, 50);
}

uint16_t IRDA_checksum(uint8_t receiveString[], uint8_t size) {
	//receiveString
	// Get checksum from message (convert from ASCII to binary)¨
	uint16_t checksum = (((uint16_t) receiveString[size - 7]) - 0x0030) * 1000;
	checksum += (((uint16_t) receiveString[size - 6]) - 0x0030) * 100;
	checksum += (((uint16_t) receiveString[size - 5]) - 0x0030) * 10;
	checksum += ((uint16_t) receiveString[size - 4]) - 0x0030;

	uint16_t checksumCalculated = 0;

	for (uint8_t i = 0; i < size - 10; i++) {
		checksumCalculated += receiveString[i];
	}

	checksumCalculated += 8 * 0x0020 + receiveString[size - 2];

	uint8_t message1[] = {checksumCalculated >> 8, checksumCalculated};
	uint8_t message2[] = {checksum >> 8, checksum};

	HAL_IRDA_Transmit(&hirda1, message1, 2, 50);
	HAL_IRDA_Transmit(&hirda1, message2, 2, 50);

	return checksum << 8 | checksumCalculated;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_LoRaWAN_Init();
  /* USER CODE BEGIN 2 */
	MX_USART2_UART_Init();
	MX_USART1_IRDA_Init();

	UTIL_TIMER_Create(&wakeUpSendTimer, 2000, UTIL_TIMER_PERIODIC, IRDA_Transmit_Wake_Up, NULL);
	UTIL_TIMER_Start(&wakeUpSendTimer);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	uint8_t returnCode = -1;
	uint8_t message = -1;

	// Start waking up the sensor
	message = MESSAGE_WAKE_UP;

	while (1) {
		returnCode = IRDA_Receive(hirda1);

		// Reception is complete
		if (returnCode) {
			// Get received content in string form, so that it can be compared
			char receiveString[rxPointer];

			strncpy(receiveString, rxBuffer, rxPointer);

			// If message contains checksum
			if (strstr(receiveString, "K23") != NULL) {
				uint32_t result = IRDA_checksum(receiveString, rxPointer);

				/*
				 * ADD CODE FOR CHECKSUM
				 */
			}

			// Once woken up, sensor will ask "what function" with "08?\r"
			if (!strcmp(receiveString, "?08\r")) {
				// Stop timer
				UTIL_TIMER_Stop(&wakeUpSendTimer);

				// Send request
				uint8_t messageSend[] = "S\r";

				HAL_IRDA_Transmit(&hirda1, messageSend, sizeof(messageSend) - 1, 50);

				message = MESSAGE_START;
			} else if (!strcmp(receiveString, "*\r")) {
				// Acknowledge that there is a send request ("S")
				if (message == MESSAGE_START) {
					// Send first register value request
					uint8_t messageSend[] = "F0017G0010\r";

					HAL_IRDA_Transmit(&hirda1, messageSend, sizeof(messageSend) - 1, 50);
				}

				// Acknowledge the end of transmission ("A", Abbruch)
				if (message == MESSAGE_END) {
					// Done, can go back to sleep
				}
			} else if (!strncmp(receiveString, "K85 00170010", 12)) {
				// Register value requests
				uint8_t messageSend[] = "F0017G0020\r";

				HAL_IRDA_Transmit(&hirda1, messageSend, sizeof(messageSend) - 1, 50);
			} else if (!strncmp(receiveString, "K85 00170020", 12)) {
				uint8_t messageSend[] = "F0017G0030\r";

				HAL_IRDA_Transmit(&hirda1, messageSend, sizeof(messageSend) - 1, 50);
			} else if (!strncmp(receiveString, "K85 00170030", 12)) {
				uint8_t messageSend[] = "F0017G0035\r";

				HAL_IRDA_Transmit(&hirda1, messageSend, sizeof(messageSend) - 1, 50);
			} else if (!strncmp(receiveString, "K85 00170035", 12)) {
				uint8_t messageSend[] = "F0017G0036\r";

				HAL_IRDA_Transmit(&hirda1, messageSend, sizeof(messageSend) - 1, 50);
			} else if (!strncmp(receiveString, "K85 00170036", 12)) {
				message = MESSAGE_END;

				uint8_t messageSend[] = "F0017G0090\r";

				HAL_IRDA_Transmit(&hirda1, messageSend, sizeof(messageSend) - 1, 50);
			} else if (!strncmp(receiveString, "K85 00170090", 12)) {
				// Last register value request
				uint8_t messageSend[] = "A\r";

				HAL_IRDA_Transmit(&hirda1, messageSend, sizeof(messageSend) - 1, 50);
			} else {
				/*
				 * Not awaited reception handling goes here
				 */
			}

			/*
			 * CODE LORA - SEND
			 */
			//SendTxData();

			rxPointer = 0;
		}

		//APP_LOG(TS_ON,VLEVEL_M,"%d\r\n", MSG_ret);

		/* USER CODE END WHILE */
		//MX_LoRaWAN_Process();
	}

    /* USER CODE BEGIN 3 */

	/* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE|RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_11;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the SYSCLKSource, HCLK, PCLK1 and PCLK2 clocks dividers
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK3|RCC_CLOCKTYPE_HCLK
                              |RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1
                              |RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.AHBCLK3Divider = RCC_SYSCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */



/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
