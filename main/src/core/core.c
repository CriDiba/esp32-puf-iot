#include "core/core.h"
#include "net/wifi.h"
#include "net/http_client.h"
#include "console/console.h"
#include "net/http_client.h"
#include "net/mqtt.h"
#include "define.h"
#include "puflib.h"
#include "core/nvs.h"
#include "core/error.h"
#include "crypto/crypto.h"

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

#define STATE_UPDATE_INTERVAL 2000

static const char *TAG = "Core";

/* core task event loop */
esp_event_loop_handle_t loop_core_task;

ESP_EVENT_DEFINE_BASE(CORE_EVENT);

static struct
{
    CoreState state;
    uint32_t lastStatePutTimestamp;
    bool isCertificateRotationEnabled;
} coreState = {0};

void Core_SetCoreState(CoreState newState);
static void Core_TaskMain(void *pvParameters);
static void Core_EnrollPuf(void);
static void Core_CloudConnect();
static void Core_CloudProcessLoop(uint32_t timestamp);
static void Core_CloudCallback(CString topic, CBuffer payload);
static void Core_OnChallenge(char *challenge);
static void Core_OnCreateCSR();
static void Core_OnReceiveCRT(CBuffer certPayload);
static void Core_OnRotateCRT();

static uint32_t Time_GetTimeMs();

static void Core_EventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch ((CoreEvent)event_id)
    {
    case CORE_EVENT_ENROLL:
        Core_EnrollPuf();
        break;
    case CORE_EVENT_CONNECTED:
        Core_SetCoreState(CORE_STATE_ONLINE);
        /* start tcp console */
        Console_TaskStart();
        break;

    case CORE_EVENT_PUF_CHALLENGE:
    {
        char *chall = (char *)event_data;
        ESP_LOGI(TAG, "received new challenge %s", chall);
        Core_OnChallenge(chall);
    }
    break;

    case CORE_EVENT_CERT_ROTATION:

        break;
    default:
        ESP_LOGI(TAG, "unhandled core event: %d", event_id);
        break;
    }
}

static void Core_CloudConnect()
{
    /* connect to mqtt broker */
    bool sessionPresent = false;
    ErrorCode err = Mqtt_Connect(mkCSTRING(DEVICE_ID), &sessionPresent, true);

    if (err)
        return;

    Core_SetCoreState(CORE_STATE_CLOUD_CONNECTED);

    if (!sessionPresent)
    {
        Mqtt_Subscribe(mkCSTRING(MGT_TOPIC_FILTER));
    }
}

static void Core_CloudProcessLoop(uint32_t timestamp)
{
    ESP_LOGI(TAG, "QUI");
    // send periodic state update
    if (timestamp - coreState.lastStatePutTimestamp > STATE_UPDATE_INTERVAL)
    {
        ESP_LOGI(TAG, "send periodic state update to cloud");

        char topicBuf[50] = {0};
        CString topic = {
            .string = topicBuf,
            .length = snprintf(topicBuf, 50, "%s/temperature", DEVICE_ID)};

        char *data = "{\"temp\": \"12.8\"}";
        CBuffer buffer = {.buffer = (uint8_t *)data, .length = strlen(data)};
        Mqtt_Publish(topic, buffer);
        coreState.lastStatePutTimestamp = timestamp;
    }

    // process mqtt loop
    Mqtt_ProcessLoop();
}

static void Core_CloudCallback(CString topic, CBuffer payload)
{
    /* this callback is invoked at each publish message received from MQTT */
    ESP_LOGI(TAG, "received message from cloud");
    printf("TOPIC=%.*s\r\n", topic.length, topic.string);
    printf("DATA=%.*s\r\n", payload.length, payload.buffer);

    if (strncmp(topic.string, CSR_REQ_TOPIC, topic.length) == 0)
    {
        /* received CSR request*/
        Core_OnCreateCSR();
    }

    if (strncmp(topic.string, CRT_REQ_TOPIC, topic.length) == 0)
    {
        /* received signed certificate */
        Core_OnReceiveCRT(payload);
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

static void Core_OnCreateCSR()
{
    /* create temporary cert */
    Buffer csr;
    ErrorCode err = Crypto_RefreshCertificate(&csr);

    if (!err)
    {
        size_t dataBufMaxSize = 20 + csr.length;
        uint8_t *dataBuf = (uint8_t *)calloc(dataBufMaxSize, sizeof(uint8_t));
        size_t dataLen = snprintf((char *)dataBuf, dataBufMaxSize, "{\"csr\": \"%.*s\"}", csr.length, csr.buffer);
        CBuffer data = {.buffer = dataBuf, .length = dataLen};
        CString topic = mkCSTRING(CSR_RES_TOPIC);
        Mqtt_Publish(topic, data);
        free(dataBuf);

        coreState.isCertificateRotationEnabled = true;
    }

    free(csr.buffer);
}

static void Core_OnReceiveCRT(CBuffer certPayload)
{
    /* parse cert message */
    char *certRawStr = (char *)calloc(certPayload.length, sizeof(char));
    memcpy(certRawStr, certPayload.buffer, certPayload.length);
    char *certStr = strtok(certRawStr, "\"certificatePem\":\"");
    certStr = strtok(NULL, "\"");

    /* save temporary cert */
    Buffer cert = {.buffer = (uint8_t *)certStr, .length = strlen(certStr)};
    Nvs_SetBuffer(NVS_DEVICE_CERT_KEY_TMP, cert);

    /* disconnect from cloud */
    ErrorCode err = Mqtt_Disconnect(true);
    if (err)
        return;

    /* connect using new certificate */
    bool sessionPresent = false;
    err = Mqtt_Connect(mkCSTRING(DEVICE_ID), &sessionPresent, true);

    const char *dataBuf = "{}";
    CBuffer data = {.buffer = (uint8_t *)dataBuf, .length = strlen(dataBuf)};

    if (err)
    {
        /* failure, send ERR message */
        ESP_LOGI(TAG, "rotation: failure on connection with new cert");
        CString topic = mkCSTRING(CRT_ERR_TOPIC);
        Mqtt_Publish(topic, data);
        return;
    }

    /* success, rotate certificates */
    Core_OnRotateCRT();
    coreState.isCertificateRotationEnabled = false;

    /* send ACK message */
    ESP_LOGI(TAG, "rotation: successfully connected with new cert");
    CString topic = mkCSTRING(CRT_ACK_TOPIC);
    Mqtt_Publish(topic, data);
}

static void Core_OnRotateCRT()
{
    Buffer csr, crt, salt;
    bool findCsr = Nvs_GetBuffer(NVS_DEVICE_CSR_KEY_TMP, &csr);
    bool findCrt = Nvs_GetBuffer(NVS_DEVICE_CERT_KEY_TMP, &crt);
    bool findSalt = Nvs_GetBuffer(NVS_DEVICE_SALT_KEY_TMP, &salt);

    if (findCsr && findCrt && findSalt)
    {
        Nvs_SetBuffer(NVS_DEVICE_CSR_KEY, csr);
        Nvs_SetBuffer(NVS_DEVICE_CERT_KEY, crt);
        Nvs_SetBuffer(NVS_DEVICE_SALT_KEY, salt);
    }

    free(csr.buffer);
    free(crt.buffer);
    free(salt.buffer);
}

const char *Core_GetCrtNvsKey()
{
    if (coreState.isCertificateRotationEnabled)
        return NVS_DEVICE_CERT_KEY_TMP;
    return NVS_DEVICE_CERT_KEY;
}

const char *Core_GetCsrNvsKey()
{
    if (coreState.isCertificateRotationEnabled)
        return NVS_DEVICE_CSR_KEY_TMP;
    return NVS_DEVICE_CSR_KEY;
}

const char *Core_GetSaltNvsKey()
{
    if (coreState.isCertificateRotationEnabled)
        return NVS_DEVICE_SALT_KEY_TMP;
    return NVS_DEVICE_SALT_KEY;
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

    /* setup mqtt */
    Mqtt_Init(Core_CloudCallback);
}

static uint32_t Time_GetTimeMs()
{
    struct timeval tv = {0};
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
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

    uint32_t timestamp = 0;

    while (true)
    {
        ESP_LOGI(TAG, "running core task");
        ESP_LOGI(TAG, "STATE: %d\n", coreState.state);

        timestamp = Time_GetTimeMs();

        switch (coreState.state)
        {
        case CORE_STATE_ONLINE:
            /* connect to cloud */
            Core_CloudConnect();
            break;
        case CORE_STATE_CLOUD_CONNECTED:
            Core_CloudProcessLoop(timestamp);
        default:
            break;
        }

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

void Core_EventNotifyData(CoreEvent event_id, void *event_data, size_t event_data_size)
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