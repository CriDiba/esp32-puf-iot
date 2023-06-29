
#include "esp_err.h"
#include "esp_check.h"

typedef esp_err_t ErrorCode;

#define SUCCESS ESP_OK
#define FAILURE ESP_FAIL

#define ERROR_CHECK(x) ESP_RETURN_ON_ERROR(x, "", "")