/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "lcd_i2c_hd44780.h"
#include "stdarg.h"
#include "string.h"
#include <ctype.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
//---------------USART RX-----------------//
#define BUFF_SIZE_RX 384
//---------------USART TX-----------------//
#define BUFF_SIZE_TX 128
//---------------FRAME-----------------//
#define FRAME_START 0x00
#define FRAME_DEST 0x01
#define FRAME_SRC 0x02
#define FRAME_CMD 0x03
#define FRAME_LEN1 0x04
#define FRAME_LEN2 0x05
#define FRAME_DATA 0x06
#define FRAME_STOP 0x07
//---------------FRAME ERRORS-----------------//
#define FRAME_ERR_DEST 0x08
#define FRAME_ERR_SRC 0x09
#define FRAME_ERR_CMD 0x10
#define FRAME_ERR_LEN 0x11
#define FRAME_ERR_END 0x12
#define FRAME_ERR_DATA 0x14
#define FRAME_VALID 0x13
//---------------FRAME SIZE-----------------//
#define FRAME_DEST_SIZE 3
#define FRAME_SRC_SIZE 3
#define FRAME_CMD_SIZE 3
#define FRAME_DATA_SIZE 256
//---------------FRAME CHARS-----------------//
#define FRAME_SIGN_START 0x24
#define FRAME_SING_STOP 0x26

char answerMessage[BUFF_SIZE_RX];
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

RTC_HandleTypeDef hrtc;

TIM_HandleTypeDef htim14;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
//---------------USART RX-----------------//
uint8_t BUFFRX[BUFF_SIZE_RX];
volatile uint8_t Rxempty = 0;
volatile uint8_t Rxbusy = 0;
//---------------USART TX-----------------//
uint8_t BUFFTX[BUFF_SIZE_TX];
volatile uint8_t Txempty = 0;
volatile uint8_t Txbusy = 0;
//---------------FRAME-----------------//
uint8_t frameDest[FRAME_DEST_SIZE];
uint8_t frameSrc[FRAME_SRC_SIZE];
uint8_t frameCmd[FRAME_CMD_SIZE];
uint8_t frameLen1;
uint8_t frameLen2;
uint8_t frameTotalLen;
uint8_t frameData[FRAME_DATA_SIZE];
uint8_t frameFlag;
uint8_t wordLen = 0;

//---------------FRAME ITERATORS-----------------//
uint8_t dataIterator = 0;
uint8_t err_number = 0;
uint8_t cmd_number = 0;

/* Private variables ---------------------------------------------------------*/
char time[10];
char date[10];
uint8_t hours = 0x20;
uint8_t minutes = 0x20;
uint8_t seconds = 0x20;
uint8_t day = 0x20;
uint8_t year = 0x20;
uint8_t month = 0x02;
uint8_t alarm =0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_RTC_Init(void);
static void MX_TIM14_Init(void);
/* USER CODE BEGIN PFP */
//---------------USARTRX-----------------//
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
	if(huart -> Instance == USART2){
		Rxempty++;
		if(Rxempty >= BUFF_SIZE_RX){
			Rxempty = 0;
		}
		HAL_UART_Receive_IT(&huart2, &BUFFRX[Rxempty], 1);
	}
}
//---------------USARTTX-----------------//
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart){
	if(Txempty != Txbusy){
		uint8_t tmp = BUFFTX[Txbusy];
		Txbusy++;
		if(Txbusy >= BUFF_SIZE_TX){
			Txbusy = 0;
		}
		HAL_UART_Transmit_IT(&huart2, &tmp, 1);
	}
}

void SendData(char* format, ...){

	char TAB_TMP[BUFF_SIZE_TX];
	int idx;
	idx = Txempty;

	va_list arglist;
	va_start(arglist, format);
	vsiprintf(TAB_TMP, format, arglist);
	va_end(arglist);

	for(int i = 0; i < strlen(TAB_TMP); i++){
		BUFFTX[idx] = TAB_TMP[i];
		idx++;
		if(idx >= BUFF_SIZE_TX){
			idx = 0;
		}
	}

	__disable_irq();

	if((Txempty == Txbusy) && (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TXE) == SET)){
		Txempty = idx;
		HAL_UART_Transmit_IT(&huart2, &BUFFTX[Txbusy], 1);
		Txbusy++;
		if(Txbusy >= BUFF_SIZE_TX){
			Txbusy = 0;
		}
	}else{
		Txempty = idx;
	}

	__enable_irq();
}

//---------------FUNKCJE RAMKI-----------------//
uint8_t convertToDec(char ascii){
	if(ascii >= 0x41 && ascii <= 0x46){
		return ascii - 'A' + 10;
	}else if(ascii >= 0x30 && ascii <= 0x39){
		return ascii - '0';
	}
	return -1;
}

void incrementRxBusy(){
	if(++Rxbusy > BUFF_SIZE_RX){
		Rxbusy = 0;
	}
}

uint8_t checkCapitalLetters(uint8_t letter){
	if(letter >= 0x41 && letter <= 0x5A){
		return 1;
	}else{
		return 0;
	}
}

uint8_t checkNumbers(uint8_t number){
	if(number >= 0x30 && number <= 0x39){
		return 1;
	}else{
		return 0;
	}
}

uint8_t checkCapitalLettersLen(uint8_t letter){
	if(letter >= 0x41 && letter <= 0x46){
		return 1;
	}else{
		return 0;
	}
}

uint8_t checkDEST(){
	if(!strcmp("STM", (char *)frameDest)){
		return 1;
	}else{
		return 0;
	}
}

uint8_t checkCommandExists(){
	if(!strcmp("TIM", (char *)frameCmd)){
		cmd_number = 1;
		return 1;
	}else if(!strcmp("DAT", (char *)frameCmd)){
		cmd_number = 2;
		return 1;
	}else if(!strcmp("ALA", (char *)frameCmd)){
		cmd_number = 3;
		return 1;
	}else if(!strcmp("CLR", (char *)frameCmd)){
		cmd_number = 4;
		return 1;
	}
	else{
		return 0;
	}
}


// ====================== ANALIZA RAMKI ======================
void analizeFrame(){
	char byte = BUFFRX[Rxbusy];
	if(byte == FRAME_SIGN_START){
		frameFlag = FRAME_DEST;
		incrementRxBusy();
		dataIterator = 0;
		return;
	}

	if(frameFlag==FRAME_DEST){
		if(dataIterator != FRAME_DEST_SIZE){
			if(checkCapitalLetters(byte) == 1){
				frameDest[dataIterator] = byte;
				dataIterator++;
			}else{
				frameFlag = FRAME_ERR_DEST;
				dataIterator = 0;
			}
			incrementRxBusy();
		}else{
			frameDest[dataIterator] = '\0';
			if(checkDEST() == 1){
				frameFlag = FRAME_SRC;
			}else{
				frameFlag = FRAME_ERR_DEST;
			}
			dataIterator = 0;
		}
	}
	else if(frameFlag==FRAME_SRC){
		if(dataIterator != FRAME_SRC_SIZE){
			if(checkCapitalLetters(byte) == 1){
				frameSrc[dataIterator] = byte;
				dataIterator++;
			}else{
				frameFlag = FRAME_ERR_SRC;
				dataIterator = 0;
			}
			incrementRxBusy();
		}else{
			frameSrc[dataIterator] = '\0';
			frameFlag = FRAME_CMD;
			dataIterator = 0;
		}
	}
	else if(frameFlag==FRAME_CMD){
		if(dataIterator != FRAME_CMD_SIZE){
			if(checkCapitalLetters(byte) == 1){
				frameCmd[dataIterator] = byte;
				dataIterator++;
			}else{
				frameFlag = FRAME_ERR_CMD;
				dataIterator = 0;
			}
			incrementRxBusy();
		}else{
			frameCmd[dataIterator] = '\0';
			if(checkCommandExists()){
				frameFlag = FRAME_LEN1;
			}else{
				frameFlag = FRAME_ERR_CMD;
			}
			dataIterator = 0;
		}
	}
	else if(frameFlag==FRAME_LEN1){
		if(checkCapitalLettersLen(byte) == 1 || checkNumbers(byte) == 1){
			frameLen1 = convertToDec(byte);
			if(frameLen1 == 0){
				frameFlag = FRAME_STOP;
			}else{
				frameFlag = FRAME_DATA;
			}
		}else{
			frameFlag = FRAME_ERR_LEN;
		}
		incrementRxBusy();
	}
	else if(frameFlag==FRAME_DATA){
		if(dataIterator != frameLen1){
			if(checkNumbers(byte) == 1){
				frameData[dataIterator] = byte;
				dataIterator++;
			}else{
				frameFlag = FRAME_ERR_DATA;
				dataIterator = 0;
			}
			incrementRxBusy();
		}else{
			frameData[dataIterator] = '\0';
			frameFlag = FRAME_STOP;
			dataIterator = 0;
		}
	}
	else if(frameFlag==FRAME_STOP)
		if(byte == FRAME_SING_STOP){
			frameFlag = FRAME_VALID;
		}else{
			frameFlag = FRAME_ERR_END;
		}
	else if(frameFlag==FRAME_START){
		incrementRxBusy();
	}
// ====================== WYWO�?YWANIE KOMEND ======================
	switch(frameFlag){
	case FRAME_VALID:
		switch(cmd_number){
		case 1:
			ExecuteTimeCmd(frameCmd, frameData);
			break;
		case 2:
			ExecuteDateCmd(frameCmd, frameData);
			break;
		case 3:
			ExecuteAlarmCmd(frameCmd, frameData);
			break;
		case 4:
			//ErrorWrongFrame();
			break;
		}
		cmd_number = 0;
		frameFlag = FRAME_START;
		break;
// ====================== ANALIZA B�?�?DÓW ======================
	case FRAME_ERR_DEST:
		sprintf(answerMessage, "$PCXSTM6ERRDST&");
		SendData(answerMessage);
		frameFlag = FRAME_START;
		break;
	case FRAME_ERR_CMD:
		sprintf(answerMessage, "$PCXSTM6ERRCMD&");
		SendData(answerMessage);
		frameFlag = FRAME_START;
		break;
	case FRAME_ERR_SRC:
		sprintf(answerMessage, "$PCXSTM6ERRSRC&");
		SendData(answerMessage);
		frameFlag = FRAME_START;
	case FRAME_ERR_LEN:
		sprintf(answerMessage, "$PCXSTM6ERRLEN&");
		SendData(answerMessage);
		frameFlag = FRAME_START;
	case FRAME_ERR_DATA:
		sprintf(answerMessage, "$PCXSTM7ERRDATA&");
		SendData(answerMessage);
		frameFlag = FRAME_START;
	case FRAME_ERR_END:
		sprintf(answerMessage, "$PCXSTM6ERREND&");
		SendData(answerMessage);
		frameFlag = FRAME_ERR_END;
	}
}
// ====================== POBIERANIE DANYCH======================
uint8_t GetTime(int idx, uint8_t *frameData) {
	uint8_t take;
	char dim[2];
	memcpy(dim, &frameData[idx], 2);
	take = atoi(dim);
	return take;
}

void ExecuteTimeCmd(char *frameCmd, uint8_t *frameData) {
	uint8_t hours = GetTime(0, frameData);
	uint8_t minutes = GetTime(2, frameData);
	uint8_t seconds = GetTime(4, frameData);
	if(hours<=24 && minutes<60 && seconds<60){
	if(hours>15){
	 hours = hours+6;
	}
	if (minutes>15 && minutes<=25){
		minutes=minutes+6;
			}
	else if (minutes>25 && minutes<=35){
			minutes=minutes+12;
			}
	else if (minutes>35 && minutes<=45){
			minutes=minutes+18;
			}
	else if (minutes>45 && minutes<=55){
			minutes=minutes+24;
			}
	else if (minutes>55 && minutes<=59){
			minutes=minutes+30;
			};

		if (seconds>15 && seconds<=25){
			seconds=seconds+6;
				}
		else if (seconds>25 && seconds<=35){
				seconds=seconds+12;
				}
		else if (seconds>35 && seconds<=45){
				seconds=seconds+18;
				}
		else if (seconds>45 && seconds<=55){
				seconds=seconds+24;
				}
		else if (seconds>55 && seconds<=59){
				seconds=seconds+30;
				};
		set_time(hours, minutes, seconds);
	}
	else{
		SendData("TIME ERROR");
	}
}
void ExecuteDateCmd(char *frameCmd, uint8_t *frameData) {
	uint8_t weekday = GetTime(0, frameData);
	uint8_t date = GetTime(2, frameData);
	uint8_t month = GetTime(4, frameData);
	uint8_t year = GetTime(6, frameData);
if(weekday<=7 && date <=31 && month<=12){
	if(weekday>15){
	 weekday = weekday+6;
	}
	if(date>15){
		date = date+6;
		}
	else if (date>15 && date<=25){
		date=date+6;
		}
	else if (date>25 && date<=35){
		date=date+12;
		}
	if(year>15){
		 year = year+6;
		}
	else if (year>15 && year<=25){
		year=year+6;
		}
	else if (year>25 && year<=35){
			year=year+12;
		};
	set_date(weekday, month, date, year);
}
else {
	SendData("DATE ERROR");
}
}
void ExecuteAlarmCmd(char *frameCmd, uint8_t *frameData) {
	uint8_t a_hours = GetTime(0, frameData);
	uint8_t a_minutes = GetTime(2, frameData);
	uint8_t a_seconds = GetTime(4, frameData);
	if(hours<=24 && minutes<60 && seconds<60){
	if(a_hours>15){
		 a_hours = a_hours+6;
		}
		if (a_minutes>15 && a_minutes<=25){
			a_minutes=a_minutes+6;
				}
		else if (a_minutes>25 && a_minutes<=35){
			a_minutes=a_minutes+12;
				}
		else if (a_minutes>35 && a_minutes<=45){
			a_minutes=a_minutes+18;
				}
		else if (a_minutes>45 && a_minutes<=55){
			a_minutes=a_minutes+24;
				}
		else if (a_minutes>55 && a_minutes<=59){
			a_minutes=a_minutes+30;
				};
		if (a_seconds>15 && a_seconds<=25){
				a_seconds=a_seconds+6;
					}
			else if (a_seconds>25 && a_seconds<=35){
				a_seconds=a_seconds+12;
					}
			else if (a_seconds>35 && a_seconds<=45){
				a_seconds=a_seconds+18;
					}
			else if (a_seconds>45 && a_seconds<=55){
				a_seconds=a_seconds+24;
					}
			else if (a_seconds>55 && a_seconds<=59){
				a_seconds=a_seconds+30;
					};
	set_alarm(a_hours, a_minutes, a_seconds);
}
	else {
		SendData("ALARM ERROR");
	}
}
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void set_time (uint8_t hours, uint8_t minutes, uint8_t seconds){
	RTC_TimeTypeDef sTime;

	/** Initialize RTC and set the Time and Date
	  */
	  sTime.Hours = hours;
	  sTime.Minutes = minutes;
	  sTime.Seconds = seconds;
	  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
	  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
	  {
	    Error_Handler();
	  }

}
void set_date(uint8_t weekday, uint8_t month, uint8_t date, uint8_t year){
	RTC_DateTypeDef sDate;
	  sDate.WeekDay = weekday;
	  sDate.Month = month;
	  sDate.Date = date;
	  sDate.Year = year;

	  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
	  {
	    Error_Handler();
	  }
}

void set_alarm (uint8_t a_hours, uint8_t a_minutes, uint8_t a_seconds)
{
	  RTC_AlarmTypeDef sAlarm;
	   /**Enable the Alarm A
	    */
	  sAlarm.AlarmTime.Hours = a_hours;
	  sAlarm.AlarmTime.Minutes = a_minutes;
	  sAlarm.AlarmTime.Seconds = a_seconds;
	  sAlarm.AlarmTime.SubSeconds = 0x0;
	  sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	  sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;
	  sAlarm.AlarmMask = RTC_ALARMMASK_NONE;
	  sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;
	  sAlarm.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_DATE;
	  sAlarm.AlarmDateWeekDay = 0x10;
	  sAlarm.Alarm = RTC_ALARM_A;
	  if (HAL_RTC_SetAlarm(&hrtc, &sAlarm, RTC_FORMAT_BCD) != HAL_OK)
	    {
	      Error_Handler();
	    }
  /* USER CODE BEGIN RTC_Init 5 */

  /* USER CODE END RTC_Init 5 */
}

void get_time(void)
{
  RTC_DateTypeDef gDate;
  RTC_TimeTypeDef gTime;

  /* Get the RTC current Time */
  HAL_RTC_GetTime(&hrtc, &gTime, RTC_FORMAT_BIN);
  /* Get the RTC current Date */
  HAL_RTC_GetDate(&hrtc, &gDate, RTC_FORMAT_BIN);

  /* Display time Format: hh:mm:ss */
  sprintf((char*)time,"%02d:%02d:%02d",gTime.Hours, gTime.Minutes, gTime.Seconds);

  /* Display date Format: mm-dd-yy */
  sprintf((char*)date,"%02d-%02d-%2d",gDate.Date, gDate.Month, 2000 + gDate.Year);
}


void display_time (void)
{
	LCD_Set_Position(0,0);;
	LCD_Send_String (time);
	LCD_Set_Position(0,1);
	LCD_Send_String (date);
}

void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc)
{
	alarm = 1;
}

void to_do_on_alarm (void)
{
	HAL_GPIO_WritePin (GPIOA, GPIO_PIN_5, 1);
	LCD_Set_Position(0,0);;
	LCD_Send_String ("ALARM!!!");
	LCD_Set_Position(0,1);
	LCD_Send_String ("POBUDKA!!!");
	HAL_Delay (3000);

	for (int i=0;i<60;i++)
	{
		display_time();	}
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
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  MX_RTC_Init();
  MX_TIM14_Init();
  /* USER CODE BEGIN 2 */
  HAL_UART_Receive_IT(&huart2, &BUFFRX[Rxempty], 1);
  LCD_Init();
      if(HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1) != 0x32F2)
              {
                set_time(hours, minutes, seconds);
              }
  /* USER CODE END 2 */
 
 

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  get_time();
	  display_time();
	  if (alarm)
	  	  {
	  		 to_do_on_alarm();
	  		 alarm =0;
	  	  }
	  if(Rxbusy != Rxempty){
	  		analizeFrame();
	  		  }
    /* USER CODE END WHILE */

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
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Configure the main internal regulator output voltage 
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Activate the Over-Drive mode 
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
  PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};
  RTC_AlarmTypeDef sAlarm = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */
  /** Initialize RTC Only 
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */
    
  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 0x0;
  sTime.Minutes = 0x0;
  sTime.Seconds = 0x0;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  sDate.WeekDay = RTC_WEEKDAY_MONDAY;
  sDate.Month = RTC_MONTH_JANUARY;
  sDate.Date = 0x1;
  sDate.Year = 0x0;

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  /** Enable the Alarm A
  */
  sAlarm.AlarmTime.Hours = 0x0;
  sAlarm.AlarmTime.Minutes = 0x0;
  sAlarm.AlarmTime.Seconds = 0x0;
  sAlarm.AlarmTime.SubSeconds = 0x0;
  sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;
  sAlarm.AlarmMask = RTC_ALARMMASK_NONE;
  sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;
  sAlarm.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_DATE;
  sAlarm.AlarmDateWeekDay = 0x1;
  sAlarm.Alarm = RTC_ALARM_A;
  if (HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief TIM14 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM14_Init(void)
{

  /* USER CODE BEGIN TIM14_Init 0 */

  /* USER CODE END TIM14_Init 0 */

  /* USER CODE BEGIN TIM14_Init 1 */

  /* USER CODE END TIM14_Init 1 */
  htim14.Instance = TIM14;
  htim14.Init.Prescaler = 17999;
  htim14.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim14.Init.Period = 999;
  htim14.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim14.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim14) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM14_Init 2 */

  /* USER CODE END TIM14_Init 2 */

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

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

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
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
