/**
  ******************************************************************************
  * File Name          : main.h
  * Description        : This file contains the common defines of the application
  ******************************************************************************
  *
  * Copyright (c) 2017 STMicroelectronics International N.V. 
  * All rights reserved.
  *
  * Redistribution and use in source and binary forms, with or without 
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice, 
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other 
  *    contributors to this software may be used to endorse or promote products 
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this 
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under 
  *    this license is void and will automatically terminate your rights under 
  *    this license. 
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS" 
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT 
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT 
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H
  /* Includes ------------------------------------------------------------------*/

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private define ------------------------------------------------------------*/

#define LED_Pin GPIO_PIN_13
#define LED_GPIO_Port GPIOC
#define THERMISTOR1_Pin GPIO_PIN_0
#define THERMISTOR1_GPIO_Port GPIOA
#define THERMISTOR2_Pin GPIO_PIN_1
#define THERMISTOR2_GPIO_Port GPIOA
#define THERMISTOR3_Pin GPIO_PIN_2
#define THERMISTOR3_GPIO_Port GPIOA
#define THERMISTOR4_Pin GPIO_PIN_3
#define THERMISTOR4_GPIO_Port GPIOA
#define THERMISTOR5_Pin GPIO_PIN_4
#define THERMISTOR5_GPIO_Port GPIOA
#define THERMISTOR6_Pin GPIO_PIN_5
#define THERMISTOR6_GPIO_Port GPIOA
#define DC12V_PDM1_Pin GPIO_PIN_0
#define DC12V_PDM1_GPIO_Port GPIOB
#define DC12V_PDM2_Pin GPIO_PIN_1
#define DC12V_PDM2_GPIO_Port GPIOB
#define DC12V_PDM3_Pin GPIO_PIN_3
#define DC12V_PDM3_GPIO_Port GPIOB
#define DC24V_PDM1_Pin GPIO_PIN_10
#define DC24V_PDM1_GPIO_Port GPIOB
#define DC24V_PDM2_Pin GPIO_PIN_11
#define DC24V_PDM2_GPIO_Port GPIOB
#define DC24V_PDM3_Pin GPIO_PIN_12
#define DC24V_PDM3_GPIO_Port GPIOB
#define AC230V_PDM1_Pin GPIO_PIN_13
#define AC230V_PDM1_GPIO_Port GPIOB
#define AC230V_PDM2_Pin GPIO_PIN_14
#define AC230V_PDM2_GPIO_Port GPIOB
#define AC230V_PDM3_Pin GPIO_PIN_15
#define AC230V_PDM3_GPIO_Port GPIOB
#define USB_DP_PULLUP_Pin GPIO_PIN_5
#define USB_DP_PULLUP_GPIO_Port GPIOB
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

/**
  * @}
  */ 

/**
  * @}
*/ 

#endif /* __MAIN_H */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
