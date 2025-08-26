
/**
  ******************************************************************************
  * @file    app_x-cube-ai.c
  * @author  X-CUBE-AI C code generator
  * @brief   AI program body
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

 /*
  * Description
  *   v1.0 - Minimum template to show how to use the Embedded Client API
  *          model. Only one input and one output is supported. All
  *          memory resources are allocated statically (AI_NETWORK_XX, defines
  *          are used).
  *          Re-target of the printf function is out-of-scope.
  *   v2.0 - add multiple IO and/or multiple heap support
  *
  *   For more information, see the embeded documentation:
  *
  *       [1] %X_CUBE_AI_DIR%/Documentation/index.html
  *
  *   X_CUBE_AI_DIR indicates the location where the X-CUBE-AI pack is installed
  *   typical : C:\Users\[user_name]\STM32Cube\Repository\STMicroelectronics\X-CUBE-AI\7.1.0
  */

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/

#if defined ( __ICCARM__ )
#elif defined ( __CC_ARM ) || ( __GNUC__ )
#endif

/* System headers */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "app_x-cube-ai.h"
#include "main.h"
#include "ai_datatypes_defines.h"
#include "network.h"
#include "network_data.h"

/* USER CODE BEGIN includes */
#include "model_data.h"
#include <math.h>
/* USER CODE END includes */

/* IO buffers ----------------------------------------------------------------*/

#if !defined(AI_NETWORK_INPUTS_IN_ACTIVATIONS)
AI_ALIGNED(4) ai_i8 data_in_1[AI_NETWORK_IN_1_SIZE_BYTES];
ai_i8* data_ins[AI_NETWORK_IN_NUM] = {
data_in_1
};
#else
ai_i8* data_ins[AI_NETWORK_IN_NUM] = {
NULL
};
#endif

#if !defined(AI_NETWORK_OUTPUTS_IN_ACTIVATIONS)
AI_ALIGNED(4) ai_i8 data_out_1[AI_NETWORK_OUT_1_SIZE_BYTES];
ai_i8* data_outs[AI_NETWORK_OUT_NUM] = {
data_out_1
};
#else
ai_i8* data_outs[AI_NETWORK_OUT_NUM] = {
NULL
};
#endif

/* Activations buffers -------------------------------------------------------*/

AI_ALIGNED(32)
static uint8_t pool0[AI_NETWORK_DATA_ACTIVATION_1_SIZE];

ai_handle data_activations0[] = {pool0};

/* AI objects ----------------------------------------------------------------*/

static ai_handle network = AI_HANDLE_NULL;

static ai_buffer* ai_input;
static ai_buffer* ai_output;

static void ai_log_err(const ai_error err, const char *fct)
{
  /* USER CODE BEGIN log */
  if (fct)
    printf("TEMPLATE - Error (%s) - type=0x%02x code=0x%02x\r\n", fct, err.type,
           err.code);
  else
    printf("TEMPLATE - Error - type=0x%02x code=0x%02x\r\n", err.type,
           err.code);

  do {
  } while (1);
  /* USER CODE END log */
}

static int ai_boostrap(ai_handle *act_addr)
{
  ai_error err;

  /* Create and initialize an instance of the model */
  err = ai_network_create_and_init(&network, act_addr, NULL);
  if (err.type != AI_ERROR_NONE) {
    ai_log_err(err, "ai_network_create_and_init");
    return -1;
  }

  ai_input = ai_network_inputs_get(network, NULL);
  ai_output = ai_network_outputs_get(network, NULL);

#if defined(AI_NETWORK_INPUTS_IN_ACTIVATIONS)
  /*  In the case where "--allocate-inputs" option is used, memory buffer can be
   *  used from the activations buffer. This is not mandatory.
   */
  for (int idx=0; idx < AI_NETWORK_IN_NUM; idx++) {
	data_ins[idx] = ai_input[idx].data;
  }
#else
  for (int idx=0; idx < AI_NETWORK_IN_NUM; idx++) {
	  ai_input[idx].data = data_ins[idx];
  }
#endif

#if defined(AI_NETWORK_OUTPUTS_IN_ACTIVATIONS)
  /*  In the case where "--allocate-outputs" option is used, memory buffer can be
   *  used from the activations buffer. This is no mandatory.
   */
  for (int idx=0; idx < AI_NETWORK_OUT_NUM; idx++) {
	data_outs[idx] = ai_output[idx].data;
  }
#else
  for (int idx=0; idx < AI_NETWORK_OUT_NUM; idx++) {
	ai_output[idx].data = data_outs[idx];
  }
#endif

  return 0;
}

static int ai_run(void)
{
  ai_i32 batch;

  batch = ai_network_run(network, ai_input, ai_output);
  if (batch != 1) {
    ai_log_err(ai_network_get_error(network),
        "ai_network_run");
    return -1;
  }

  return 0;
}

/* USER CODE BEGIN 2 */
// WiFi RSSI 데이터를 AI 모델 입력 형식으로 변환
int prepare_rssi_input(ai_i8 *data[], float *rssi_features) {
  // 입력 크기 검증
  if (AI_NETWORK_IN_1_SIZE != BSSID_COUNT) {
    printf("Error: Model input size mismatch\r\n");
    return -1;
  }

  // TFLite 모델에서 제공된 정확한 양자화 파라미터
  const float quant_scale = 0.0659424364566803f;
  const int32_t quant_zero_point = -61;

  int non_default_count = 0;

  // RSSI 특성을 AI 모델의 S8 입력 형식으로 변환
  for (int i = 0; i < BSSID_COUNT; i++) {
    float raw_rssi = rssi_features[i];

    if (raw_rssi > -100.0f) {
      non_default_count++;
    }

    // 1. StandardScaler 적용
    // model_data.h 에 scaler_mean, scaler_scale 배열이 있어야 합니다.
    float scaled_rssi = (raw_rssi - scaler_mean[i]) / scaler_scale[i];

    // 2. 양자화 적용
    int32_t temp_val =
        (int32_t)roundf(scaled_rssi / quant_scale) + quant_zero_point;

    // int8 범위(-128 ~ 127)로 클램핑
    if (temp_val < -128)
      temp_val = -128;
    if (temp_val > 127)
      temp_val = 127;

    data[0][i] = (int8_t)temp_val;
  }

  printf("Input: %d/%d active APs\r\n", non_default_count, BSSID_COUNT);

  // 샘플 입력값 출력
  printf("Sample inputs (first 5 active): ");
  int sample_count = 0;
  for (int i = 0; i < BSSID_COUNT && sample_count < 5; i++) {
    if (rssi_features[i] > -100.0f) {
      printf("i%d=%d ", i, data[0][i]);
      sample_count++;
    }
  }
  printf("\r\n");

  return 0;
}

int process_zone_prediction(ai_i8 *data[]) {
  if (!data || !data[0]) {
    printf("Error: Invalid output data\r\n");
    return -1;
  }

  // 출력 양자화 파라미터 (TFLite 변환 시 결정됨)
  const float dequant_scale = 0.003906250f;
  const int32_t dequant_zero_point = -128;

  float logits[AI_NETWORK_OUT_1_SIZE];
  printf("Raw Output: [");
  for (int i = 0; i < AI_NETWORK_OUT_1_SIZE; i++) {
    // 역양자화: x = (y - zero_point) * scale
    logits[i] = ((float)data[0][i] - dequant_zero_point) * dequant_scale;
    printf("%d", data[0][i]);
    if (i < AI_NETWORK_OUT_1_SIZE - 1)
      printf(", ");
  }
  printf("]\r\n");

  // Softmax 계산
  float probabilities[AI_NETWORK_OUT_1_SIZE];
  float sum_exp = 0.0f;
  float max_logit = logits[0];

  for (int i = 1; i < AI_NETWORK_OUT_1_SIZE; i++) {
    if (logits[i] > max_logit) {
      max_logit = logits[i];
    }
  }

  for (int i = 0; i < AI_NETWORK_OUT_1_SIZE; i++) {
    probabilities[i] = expf(logits[i] - max_logit);
    sum_exp += probabilities[i];
  }

  // 확률 정규화 및 출력 (정수형으로)
  printf("Probabilities: [");
  for (int i = 0; i < AI_NETWORK_OUT_1_SIZE; i++) {
    if (sum_exp > 0) {
      probabilities[i] /= sum_exp;
    }
    printf("%d%%", (int)(probabilities[i] * 100));
    if (i < AI_NETWORK_OUT_1_SIZE - 1)
      printf(", ");
  }
  printf("]\r\n");

  // 가장 높은 확률의 zone 찾기
  int predicted_zone = 0;
  float max_prob = probabilities[0];
  for (int i = 1; i < AI_NETWORK_OUT_1_SIZE; i++) {
    if (probabilities[i] > max_prob) {
      max_prob = probabilities[i];
      predicted_zone = i;
    }
  }

  printf("Predicted Zone: %d (confidence: %d%%)\r\n", predicted_zone,
         (int)(max_prob * 100));

  return predicted_zone;
}

// 외부에서 호출할 수 있는 zone 예측 함수
int predict_zone_with_ai(float *rssi_features) {
  if (!network) {
    printf("Error: AI model not initialized\r\n");
    return -1;
  }

  printf("Starting zone prediction with AI model...\r\n");

  // 1. RSSI 특성을 AI 입력 형식으로 변환
  if (prepare_rssi_input(data_ins, rssi_features) == 0) {
    // 2. AI 모델 추론 실행
    if (ai_run() == 0) {
      // 3. 결과 처리 및 zone 반환
      return process_zone_prediction(data_outs);
    }
  }

  printf("Zone prediction failed\r\n");
  return -1;
}
/* USER CODE END 2 */

/* Entry points --------------------------------------------------------------*/

void MX_X_CUBE_AI_Init(void)
{
    /* USER CODE BEGIN 5 */
  printf("AI Model Initializing...\r\n");
  printf("Model file: model.tflite\r\n");

  int init_result = ai_boostrap(data_activations0);
  if (init_result == 0) {
    printf("AI Model Ready (In:%d, Out:%d)\r\n", AI_NETWORK_IN_1_SIZE,
           AI_NETWORK_OUT_1_SIZE);
    printf("Model params: 36,292 items (36.02 KiB)\r\n");
    printf("MACC operations: 36,352\r\n");
  } else {
    printf("AI Model Init FAILED!\r\n");
  }
    /* USER CODE END 5 */
}

void MX_X_CUBE_AI_Process(void)
{
    /* USER CODE BEGIN 6 */
  // AI 모델 처리 함수 - 현재는 사용하지 않음
    /* USER CODE END 6 */
}
#ifdef __cplusplus
}
#endif
