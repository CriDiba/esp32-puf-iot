#pragma once

#include "esp_event.h"

ESP_EVENT_DECLARE_BASE(CORE_EVENT);

typedef enum CoreEvent
{
    CORE_EVENT_CONNECTED,
    CORE_EVENT_ENROLL,
    CORE_EVENT_PUF_CHALLENGE,
    CORE_EVENT_CERT_ROTATION,
    CORE_EVENT_CERT_REFRESH,
} CoreEvent;

typedef enum CoreState
{
    CORE_STATE_IDLE,
    CORE_STATE_NOT_ENROLLED,
    CORE_STATE_ENROLLED,
    CORE_STATE_ONLINE,
    CORE_STATE_CLOUD_CONNECTED,

} CoreState;

void Core_TaskStart(void);
void Core_EventNotifyData(CoreEvent event_id, void *event_data, size_t event_data_size);
void Core_EventNotify(CoreEvent event_id);

const char *Core_GetCrtNvsKey();
const char *Core_GetCsrNvsKey();
const char *Core_GetSaltNvsKey();