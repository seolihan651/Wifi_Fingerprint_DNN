/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "app_x-cube-ai.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "model_data.h"
#include "network.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c2;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
// ESP01 통신용 버퍼
#define ESP_BUFFER_SIZE 4096
char esp_rx_buffer[ESP_BUFFER_SIZE];
char esp_tx_buffer[256];
volatile uint16_t esp_rx_index = 0;
volatile uint8_t esp_response_ready = 0;
volatile uint8_t esp_rx_char;

// WiFi 스캔 결과 저장용
typedef struct {
    char bssid[18];  // MAC 주소 형식: "xx:xx:xx:xx:xx:xx"
    int rssi;
} wifi_ap_t;

#define MAX_AP_COUNT 25
wifi_ap_t scanned_aps[MAX_AP_COUNT];
uint8_t ap_count = 0;

// LCD I2C 주소 및 타이밍 변수
#define LCD_ADDR 0x27 << 1  // I2C LCD 주소 (일반적으로 0x27)
uint32_t last_scan_time = 0;
#define SCAN_INTERVAL_MS 10000  // 10초마다 스캔

// 현재 추론 결과 저장
int current_zone = -1;
uint8_t zone_updated = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C2_Init(void);
/* USER CODE BEGIN PFP */
// ESP01 WiFi 관련 함수들
void esp01_init(void);
void esp01_send_command(const char* cmd);
uint8_t esp01_wait_response(const char* expected, uint32_t timeout_ms);
void esp01_scan_wifi(void);
void parse_wifi_scan_result(const char* response);
void esp01_start_receive(void);
uint8_t esp01_check_response(const char* expected);

// WiFi 스캔 결과를 AI 모델로 처리
void estimate_zone_from_wifi_scan(void);
void create_rssi_feature_vector(float* feature_vector);
int find_bssid_index(const char* bssid);

// I2C LCD 관련 함수들
void lcd_init(void);
void lcd_clear(void);
void lcd_set_cursor(uint8_t row, uint8_t col);
void lcd_print(const char* str);
void lcd_display_zone_result(int zone, int ap_count);
void lcd_send_cmd(uint8_t cmd);
void lcd_send_data(uint8_t data);
void lcd_send_nibble(uint8_t nibble);
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
  MX_USART1_UART_Init();
  MX_I2C2_Init();
  MX_X_CUBE_AI_Init();
  /* USER CODE BEGIN 2 */
  
  // 시스템 초기화
  printf("System Initializing...\r\n");
  
  // LCD 초기화
  HAL_Delay(1000);
  lcd_init();
  lcd_clear();
  lcd_set_cursor(0, 0);
  lcd_print("AI Zone System");
  lcd_set_cursor(1, 0);
  lcd_print("Starting...");
  
  // ESP01 초기화
  esp01_init();
  esp01_start_receive();
  
  // 초기화 완료 표시
  lcd_clear();
  lcd_set_cursor(0, 0);
  lcd_print("System Ready");
  lcd_set_cursor(1, 0);
  lcd_print("Scanning...");
  
  printf("System Ready\r\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  
  while (1)
  {
    /* USER CODE END WHILE */

  MX_X_CUBE_AI_Process();
    /* USER CODE BEGIN 3 */
  // 10초마다 WiFi 스캔 및 AI 추론 실행
    uint32_t current_time = HAL_GetTick();
    if (current_time - last_scan_time >= SCAN_INTERVAL_MS) {
      
      // LCD에 스캔 중 표시
      lcd_clear();
      lcd_set_cursor(0, 0);
      lcd_print("Scanning WiFi...");
      
      // WiFi 스캔 및 AI 추론 실행
      esp01_scan_wifi();
      
      last_scan_time = current_time;
    }
    
    // LED 토글 (시스템 동작 표시)
    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
    HAL_Delay(100);
  /* USER CODE END 3 */

  }
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 100;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.ClockSpeed = 100000;
  hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

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

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
// UART를 통한 printf 리타겟팅
int __io_putchar(int ch)
{
  HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}

// I2C LCD 함수들 (16x2 LCD with PCF8574 I2C backpack)
void lcd_send_nibble(uint8_t nibble)
{
  uint8_t data = nibble | 0x08; // Enable backlight
  HAL_I2C_Master_Transmit(&hi2c2, LCD_ADDR, &data, 1, HAL_MAX_DELAY);
  
  data |= 0x04; // Set Enable bit
  HAL_I2C_Master_Transmit(&hi2c2, LCD_ADDR, &data, 1, HAL_MAX_DELAY);
  HAL_Delay(1);
  
  data &= ~0x04; // Clear Enable bit
  HAL_I2C_Master_Transmit(&hi2c2, LCD_ADDR, &data, 1, HAL_MAX_DELAY);
  HAL_Delay(1);
}

void lcd_send_cmd(uint8_t cmd)
{
  lcd_send_nibble(cmd & 0xF0);        // Send upper nibble
  lcd_send_nibble((cmd << 4) & 0xF0); // Send lower nibble
  HAL_Delay(2);
}

void lcd_send_data(uint8_t data)
{
  lcd_send_nibble((data & 0xF0) | 0x01);        // Send upper nibble with RS=1
  lcd_send_nibble(((data << 4) & 0xF0) | 0x01); // Send lower nibble with RS=1
  HAL_Delay(2);
}

void lcd_init(void)
{
  HAL_Delay(50); // Wait for LCD to power up
  
  // Initialize LCD in 4-bit mode
  lcd_send_nibble(0x30);
  HAL_Delay(5);
  lcd_send_nibble(0x30);
  HAL_Delay(1);
  lcd_send_nibble(0x30);
  HAL_Delay(1);
  lcd_send_nibble(0x20); // Set 4-bit mode
  HAL_Delay(1);
  
  // Configure LCD
  lcd_send_cmd(0x28); // 4-bit, 2 lines, 5x8 font
  lcd_send_cmd(0x0C); // Display on, cursor off
  lcd_send_cmd(0x06); // Entry mode: increment cursor
  lcd_send_cmd(0x01); // Clear display
  HAL_Delay(2);
}

void lcd_clear(void)
{
  lcd_send_cmd(0x01);
  HAL_Delay(2);
}

void lcd_set_cursor(uint8_t row, uint8_t col)
{
  uint8_t address = (row == 0) ? 0x80 : 0xC0;
  address += col;
  lcd_send_cmd(address);
}

void lcd_print(const char* str)
{
  while (*str) {
    lcd_send_data(*str++);
  }
}

void lcd_display_zone_result(int zone, int ap_count)
{
  char line1[17], line2[17];
  
  // 첫 번째 줄: Zone 정보
  snprintf(line1, sizeof(line1), "Zone: %d", zone);
  
  // 두 번째 줄: AP 개수와 시간
  uint32_t seconds = HAL_GetTick() / 1000;
  uint32_t minutes = seconds / 60;
  seconds = seconds % 60;
  snprintf(line2, sizeof(line2), "APs:%d %02ld:%02ld", ap_count, minutes, seconds);
  
  lcd_clear();
  lcd_set_cursor(0, 0);
  lcd_print(line1);
  lcd_set_cursor(1, 0);
  lcd_print(line2);
}

// ESP01 초기화 함수
void esp01_init(void)
{
  printf("ESP01 Initializing...\r\n");
  
  // ESP01 리셋
  esp01_send_command("AT+RST\r\n");
  HAL_Delay(3000);
  
  // WiFi 모드 설정 (Station 모드)
  esp01_send_command("AT+CWMODE=1\r\n");
  HAL_Delay(1000);
  
  printf("ESP01 Ready\r\n");
}

// ESP01에 AT 명령 전송
void esp01_send_command(const char* cmd)
{
  // 수신 버퍼 초기화
  memset(esp_rx_buffer, 0, ESP_BUFFER_SIZE);
  esp_rx_index = 0;
  esp_response_ready = 0;
  
  // 명령 전송
  HAL_UART_Transmit(&huart1, (uint8_t*)cmd, strlen(cmd), 1000);
}

// UART 인터럽트 수신 시작
void esp01_start_receive(void)
{
  HAL_UART_Receive_IT(&huart1, (uint8_t*)&esp_rx_char, 1);
}

// 응답 확인 (논블로킹)
uint8_t esp01_check_response(const char* expected)
{
  if (strstr(esp_rx_buffer, expected) != NULL) {
    printf("Response found: %s\r\n", expected);
    return 1;
  }
  return 0;
}

// ESP01 응답 대기 (개선된 버전)
uint8_t esp01_wait_response(const char* expected, uint32_t timeout_ms)
{
  uint32_t start_time = HAL_GetTick();
  
  while ((HAL_GetTick() - start_time) < timeout_ms) {
    if (esp01_check_response(expected)) {
      return 1;
    }
    HAL_Delay(10); // CPU 과부하 방지
  }
  
  printf("Response timeout for: %s\r\n", expected);
  if (esp_rx_index > 0) {
    printf("Received buffer: %s\r\n", esp_rx_buffer);
  }
  return 0;
}

// WiFi 스캔 실행
void esp01_scan_wifi(void)
{
  printf("WiFi Scanning...\r\n");
  
  // 버퍼 초기화
  memset(esp_rx_buffer, 0, ESP_BUFFER_SIZE);
  esp_rx_index = 0;
  esp_response_ready = 0;
  
  // WiFi 스캔 명령 전송
  esp01_send_command("AT+CWLAP\r\n");
  
  // 스캔 결과 대기 (최대 15초)
  uint32_t start_time = HAL_GetTick();
  uint8_t scan_completed = 0;
  
  while ((HAL_GetTick() - start_time) < 15000 && !scan_completed) {
    if (strstr(esp_rx_buffer, "OK") != NULL || 
        strstr(esp_rx_buffer, "ERROR") != NULL) {
      scan_completed = 1;
      parse_wifi_scan_result(esp_rx_buffer);
      estimate_zone_from_wifi_scan();
    }
    HAL_Delay(100);
  }
  
  if (!scan_completed) {
    printf("WiFi scan timeout\r\n");
  }
}

// WiFi 스캔 결과 파싱
void parse_wifi_scan_result(const char* response)
{
  ap_count = 0;
  
  // 응답을 줄 단위로 분석
  char temp_buffer[ESP_BUFFER_SIZE];
  strncpy(temp_buffer, response, ESP_BUFFER_SIZE - 1);
  temp_buffer[ESP_BUFFER_SIZE - 1] = '\0';
  
  char* line = strtok(temp_buffer, "\r\n");
  while (line != NULL && ap_count < MAX_AP_COUNT) {
    if (strstr(line, "+CWLAP:") != NULL) {
      // +CWLAP:(3,"KCCI_STC_S",-71,"04:bd:88:f3:70:a0",1,-24,0) 형식 파싱
      //printf("Parsing line: %s\r\n", line);
      
      // BSSID 추출 (두 번째 따옴표 쌍)
      char* pos = line;
      int quote_count = 0;
      char* bssid_start = NULL;
      char* bssid_end = NULL;
      
      while (*pos) {
        if (*pos == '"') {
          quote_count++;
          if (quote_count == 3) { // 두 번째 따옴표 쌍의 시작 (BSSID)
            bssid_start = pos + 1;
          } else if (quote_count == 4) { // 두 번째 따옴표 쌍의 끝
            bssid_end = pos;
            break;
          }
        }
        pos++;
      }
      
      // RSSI 추출 (SSID 다음의 콤마 후 숫자)
      char* rssi_start = NULL;
      char* rssi_end = NULL;
      pos = line;
      int comma_count = 0;
      
      while (*pos) {
        if (*pos == ',') {
          comma_count++;
          if (comma_count == 2) { // 두 번째 콤마 후가 RSSI
            rssi_start = pos + 1;
            // RSSI 끝 찾기 (다음 콤마까지)
            char* temp_pos = rssi_start;
            while (*temp_pos && *temp_pos != ',') {
              temp_pos++;
            }
            rssi_end = temp_pos;
            break;
          }
        }
        pos++;
      }
      
      // BSSID와 RSSI가 모두 추출되었는지 확인
      if (bssid_start && bssid_end && (bssid_end - bssid_start) == 17 &&
          rssi_start && rssi_end) {
        
        // BSSID 저장
        strncpy(scanned_aps[ap_count].bssid, bssid_start, 17);
        scanned_aps[ap_count].bssid[17] = '\0';
        
        // RSSI 저장
        char rssi_str[10];
        int rssi_len = rssi_end - rssi_start;
        if (rssi_len < 10 && rssi_len > 0) {
          strncpy(rssi_str, rssi_start, rssi_len);
          rssi_str[rssi_len] = '\0';
          scanned_aps[ap_count].rssi = atoi(rssi_str);
        } else {
          scanned_aps[ap_count].rssi = -100; // 기본값
        }
        
        ap_count++;
      }
    }
    line = strtok(NULL, "\r\n");
  }
  
  printf("Found %d APs\r\n", ap_count);
}

// WiFi 스캔 결과를 AI 모델로 처리하여 zone 추정
void estimate_zone_from_wifi_scan(void)
{
  if (ap_count == 0) {
    printf("No WiFi APs detected\r\n");
    
    // LCD에 에러 표시
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_print("No WiFi APs");
    lcd_set_cursor(1, 0);
    lcd_print("Zone: Unknown");
    
    current_zone = -1;
    return;
  }
  
  printf("Processing %d APs...\r\n", ap_count);
  
  // RSSI 특성 벡터 생성 (216차원)
  static float rssi_features[BSSID_COUNT];
  create_rssi_feature_vector(rssi_features);
  
  // 실제 AI 모델로 zone 예측
  int predicted_zone = predict_zone_with_ai(rssi_features);
  
  if (predicted_zone >= 0) {
    // 결과를 LCD에 표시
    lcd_display_zone_result(predicted_zone, ap_count);
    
    current_zone = predicted_zone;
    zone_updated = 1;
  } else {
    printf("Zone estimation failed\r\n");
    
    // LCD에 에러 표시
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_print("AI Error");
    lcd_set_cursor(1, 0);
    lcd_print("Zone: Failed");
    
    current_zone = -1;
  }
}

// BSSID 인덱스 찾기
int find_bssid_index(const char* bssid)
{
  for (int i = 0; i < BSSID_COUNT; i++) {
    if (strcmp(bssid_order[i], bssid) == 0) {
      return i;
    }
  }
  return -1; // 찾지 못함
}

// RSSI 특성 벡터 생성
void create_rssi_feature_vector(float* feature_vector)
{
  // 모든 BSSID에 대해 기본값 -100 (감지되지 않은 경우)
  for (int i = 0; i < BSSID_COUNT; i++) {
    feature_vector[i] = -100.0f;
  }
  
  // 스캔된 AP들의 RSSI 값 설정
  int known_ap_count = 0;
  
  for (int i = 0; i < ap_count; i++) {
    int bssid_idx = find_bssid_index(scanned_aps[i].bssid);
    if (bssid_idx >= 0) {
      feature_vector[bssid_idx] = (float)scanned_aps[i].rssi;
      known_ap_count++;
    }
  }
  
  printf("Mapped %d/%d APs\r\n", known_ap_count, ap_count);
}

// UART 인터럽트 콜백 함수
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1) {
    // ESP01 데이터 수신
    if (esp_rx_index < ESP_BUFFER_SIZE - 1) {
      esp_rx_buffer[esp_rx_index++] = esp_rx_char;
      esp_rx_buffer[esp_rx_index] = '\0';
    }
    
    // 다음 바이트 수신 대기
    HAL_UART_Receive_IT(&huart1, (uint8_t*)&esp_rx_char, 1);
  }
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
