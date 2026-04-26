/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_hal.h"

#include "stm32g4xx_ll_adc.h"
#include "stm32g4xx_ll_dma.h"
#include "stm32g4xx_ll_rcc.h"
#include "stm32g4xx_ll_bus.h"
#include "stm32g4xx_ll_crs.h"
#include "stm32g4xx_ll_system.h"
#include "stm32g4xx_ll_exti.h"
#include "stm32g4xx_ll_cortex.h"
#include "stm32g4xx_ll_utils.h"
#include "stm32g4xx_ll_pwr.h"
#include "stm32g4xx_ll_spi.h"
#include "stm32g4xx_ll_tim.h"
#include "stm32g4xx_ll_usart.h"
#include "stm32g4xx_ll_gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define ST_LED_Pin LL_GPIO_PIN_0
#define ST_LED_GPIO_Port GPIOA
#define ADC_PER_Pin LL_GPIO_PIN_1
#define ADC_PER_GPIO_Port GPIOA
#define ADC_FREQ_Pin LL_GPIO_PIN_2
#define ADC_FREQ_GPIO_Port GPIOA
#define ADC_SUP_Pin LL_GPIO_PIN_3
#define ADC_SUP_GPIO_Port GPIOA
#define DISP_CS_Pin LL_GPIO_PIN_4
#define DISP_CS_GPIO_Port GPIOA
#define DISP_SCK_Pin LL_GPIO_PIN_5
#define DISP_SCK_GPIO_Port GPIOA
#define DISP_MISO_Pin LL_GPIO_PIN_6
#define DISP_MISO_GPIO_Port GPIOA
#define DISP_MOSI_Pin LL_GPIO_PIN_7
#define DISP_MOSI_GPIO_Port GPIOA
#define DISP_DC_Pin LL_GPIO_PIN_0
#define DISP_DC_GPIO_Port GPIOB
#define DISP_RST_Pin LL_GPIO_PIN_10
#define DISP_RST_GPIO_Port GPIOA
#define SWDIO_Pin LL_GPIO_PIN_13
#define SWDIO_GPIO_Port GPIOA
#define SWCLK_Pin LL_GPIO_PIN_14
#define SWCLK_GPIO_Port GPIOA
#define TCH_CS_Pin LL_GPIO_PIN_15
#define TCH_CS_GPIO_Port GPIOA
#define LV_TX_Pin LL_GPIO_PIN_6
#define LV_TX_GPIO_Port GPIOB
#define LV_RX_Pin LL_GPIO_PIN_7
#define LV_RX_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
