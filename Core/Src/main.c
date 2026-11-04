/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
  STATE_IDLE = 0,
  STATE_BLINK_INITIAL,
  STATE_LED_ON_WAIT,
  STATE_BLINK_FINAL
} State_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DEBOUNCE_DELAY_MS 50
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
State_t current_state = STATE_IDLE;
uint32_t tick_start = 0;
uint8_t button_press_count = 0;

/* Variabili per il debounce e rilevamento fronti */
GPIO_PinState filtered_button_state = GPIO_PIN_SET;
GPIO_PinState last_raw_button_state = GPIO_PIN_SET;
GPIO_PinState last_filtered_button_state = GPIO_PIN_SET;
uint32_t last_debounce_tick = 0;

extern UART_HandleTypeDef huart2; // UART per la stampa su terminale virtuale[cite: 1]
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
void UART_SendStatus(const char *msg);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    uint32_t current_tick = HAL_GetTick();

    // 1. Lettura grezza del pin del pulsante (es. PC13 su Nucleo)
    GPIO_PinState raw_button_state = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13);

    // 2. Logica di Debounce Software
    if (raw_button_state != last_raw_button_state) {
        last_debounce_tick = current_tick; // Reset del timer se il segnale oscilla
    }
    last_raw_button_state = raw_button_state;

    if ((current_tick - last_debounce_tick) > DEBOUNCE_DELAY_MS) {
        if (raw_button_state != filtered_button_state) {
            filtered_button_state = raw_button_state;
        }
    }

    // 3. Rilevamento fronti puliti (Pull-up: Premuto = RESET, Rilasciato = SET)
    uint8_t button_pressed_edge  = (last_filtered_button_state == GPIO_PIN_SET && filtered_button_state == GPIO_PIN_RESET);
    uint8_t button_released_edge = (last_filtered_button_state == GPIO_PIN_RESET && filtered_button_state == GPIO_PIN_SET);

    last_filtered_button_state = filtered_button_state;

    // 4. Logica sequenziale con if / else if
    if (current_state == STATE_IDLE) {
        // Al rilascio del tasto USER parte il primo lampeggio[cite: 1]
        if (button_released_edge) {
            tick_start = current_tick;
            current_state = STATE_BLINK_INITIAL;
            UART_SendStatus("Stato: Tasto rilasciato. Lampeggio iniziale (3 volte, periodo 2s, duty 30%).\r\n");
        }
    }
    else if (current_state == STATE_BLINK_INITIAL) {
        // Periodo totale = 2000 ms. Duty cycle 30% = 600 ms ON, 1400 ms OFF (ripetuto 3 volte)[cite: 1]
        uint32_t elapsed = current_tick - tick_start;
        uint32_t cycle_time = elapsed % 2000;

        if (cycle_time < 600) {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);   // LED ON (es. LD2)
        } else {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET); // LED OFF
        }

        // Controllo completamento dei 3 cicli (3 * 2000 ms = 6000 ms)[cite: 1]
        if (elapsed >= 6000) {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET); // Resta acceso fisso[cite: 1]
            UART_SendStatus("Stato: LED acceso fisso. In attesa di 2 pressioni nei prossimi 3s...\r\n");

            tick_start = current_tick;
            button_press_count = 0;
            current_state = STATE_LED_ON_WAIT;
        }
    }
    else if (current_state == STATE_LED_ON_WAIT) {
        // Conteggio delle pressioni nei successivi 3 secondi[cite: 1]
        if (button_pressed_edge) {
            button_press_count++;
            UART_SendStatus("Info: Pressione rilevata durante l'attesa.\r\n");
        }

        // Verifica scadenza della finestra temporale di 3000 ms[cite: 1]
        if ((current_tick - tick_start) >= 3000) {
            if (button_press_count >= 2) {
                tick_start = current_tick;
                current_state = STATE_BLINK_FINAL;
                UART_SendStatus("Stato: 2 pressioni ricevute! Lampeggio finale (4 volte)...\r\n");
            } else {
                HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
                current_state = STATE_IDLE;
                UART_SendStatus("Stato: Timeout o pressioni insufficienti. Spegnimento e ritorno allo stato iniziale.\r\n");
            }
        }
    }
    else if (current_state == STATE_BLINK_FINAL) {
        // Lampeggio finale per 4 volte (es. periodo 1000 ms: 500 ms ON, 500 ms OFF)[cite: 1]
        uint32_t elapsed = current_tick - tick_start;
        uint32_t cycle_time = elapsed % 1000;

        if (cycle_time < 500) {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
        } else {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
        }

        // Controllo completamento dei 4 cicli (4 * 1000 ms = 4000 ms)[cite: 1]
        if (elapsed >= 4000) {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
            current_state = STATE_IDLE;
            UART_SendStatus("Stato: Sequenza completata. LED spento, ritorno allo stato iniziale.\r\n");
        }
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
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
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void UART_SendStatus(const char *msg) {
    HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
}
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
#ifdef USE_FULL_ASSERT
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
