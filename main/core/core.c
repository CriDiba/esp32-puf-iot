#include "core/core.h"
#include "net/wifi.h"
#include "net/http_client.h"
#include "console/console.h"
#include "net/http_client.h"
#include "define.h"
#include "puflib.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

#include <stdint.h>
#include <stdio.h>

#define CORE_TASK_QUEUE_SIZE 10
#define CORE_TASK_PERIOD_MS 500

static const char *TAG = "Core";

/* core task event loop */
esp_event_loop_handle_t loop_core_task;

ESP_EVENT_DEFINE_BASE(CORE_EVENT);

static struct {
    CoreState state;
} coreState;

void Core_SetCoreState(CoreState newState);
static void Core_EnrollPuf(void);
static void Core_OnChallenge(char *challenge);

static void Core_EventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    switch ((CoreEvent)event_id)
    {
    case CORE_EVENT_ENROLL:
        Core_EnrollPuf();
        break;
    case CORE_EVENT_CONNECTED:
        /* start tcp console */
        Console_TaskStart();
        Core_SetCoreState(CORE_STATE_ONLINE);
        break;
    case CORE_EVENT_PUF_CHALLENGE:
        {
            char* chall = (char*) event_data;
            ESP_LOGI(TAG, "received new challenge %s", chall);
            Core_OnChallenge(chall);
        }
        break;
    default:
        ESP_LOGI(TAG, "unhandled core event: %d", event_id);
        break;
    }
}

static void Core_OnChallenge(char *challenge)
{
    uint8_t response[STR_CHALL_MAX_LEN] = {0};
    char resHex[STR_CHALL_MAX_LEN * 2] = {0};
    char *resHexPtr = resHex;

    bool puf_ok = get_puf_response();
    if (PUF_STATE == RESPONSE_READY && puf_ok)
    {
        for (size_t i = 0; i < strlen(challenge); i++)
        {
            response[i] = challenge[i] ^ PUF_RESPONSE[i];
            resHexPtr += snprintf(resHexPtr, 3, "%02X", response[i]);
        }
    }

    clean_puf_response();

    Http_SendResponse(resHex);
}

static void Core_EnrollPuf(void)
{
    Core_SetCoreState(CORE_STATE_NOT_ENROLLED);
    enroll_puf();
}

static void Core_TaskSetup(void)
{
    if (!is_puf_configured())
    {
        Core_EnrollPuf();
    }
    
    Core_SetCoreState(CORE_STATE_ENROLLED);

    /* start wifi connection */
    Wifi_Init();
}

static void Core_TaskMain(void *pvParameters)
{
    ESP_LOGI(TAG, "setting up core task");

    /* create event loop */
    esp_event_loop_args_t loop_core_args = {
        .queue_size = CORE_TASK_QUEUE_SIZE,
        .task_name = NULL // no task will be created
    };

    ESP_ERROR_CHECK(esp_event_loop_create(&loop_core_args, &loop_core_task));

    /* register the handler for the core events */
    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(loop_core_task, CORE_EVENT, ESP_EVENT_ANY_ID, Core_EventHandler, loop_core_task, NULL));
    
    while (true)
    {
        ESP_LOGD(TAG, "running core task");
        esp_event_loop_run(loop_core_task, 100);
        vTaskDelay(pdMS_TO_TICKS(CORE_TASK_PERIOD_MS));
    }

    /* unregister event handler */
    esp_event_handler_unregister_with(loop_core_task, CORE_EVENT, ESP_EVENT_ANY_ID, Core_EventHandler);

    /* delete event loop */
    esp_event_loop_delete(loop_core_task);

    ESP_LOGI(TAG, "deleting task core");
    
    vTaskDelete(NULL);
}

void Core_SetCoreState(CoreState newState)
{
    if (coreState.state != newState)
    {
        ESP_LOGI(TAG, "state changed: %d -> %d", coreState.state, newState);
        coreState.state = newState;
    }
}

void Core_EventNotifyData(CoreEvent event_id, void* event_data, size_t event_data_size)
{
    ESP_ERROR_CHECK(esp_event_post_to(loop_core_task, CORE_EVENT, event_id, event_data, event_data_size, portMAX_DELAY));
}

void Core_EventNotify(CoreEvent event_id) 
{
    Core_EventNotifyData(event_id, NULL, 0);
}

void Core_TaskStart(void)
{
    /* setup task core */
    Core_TaskSetup();

    /* start task core */
    xTaskCreate(Core_TaskMain, "core_task", 4096, NULL, uxTaskPriorityGet(NULL) + 1, NULL);
}