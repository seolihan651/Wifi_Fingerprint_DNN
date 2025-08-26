/* model_data.h */

#ifndef MODEL_DATA_H_
#define MODEL_DATA_H_

// 배열의 크기를 상수로 정의하면 나중에 관리하기 편합니다.
#define BSSID_COUNT 114

// extern 키워드는 "이 변수는 다른 파일에 실제 내용이 정의되어 있다"는 의미의 '선언'입니다.
extern const char* bssid_order[BSSID_COUNT];
extern const float scaler_mean[BSSID_COUNT];
extern const float scaler_scale[BSSID_COUNT];


#endif /* MODEL_DATA_H_ */