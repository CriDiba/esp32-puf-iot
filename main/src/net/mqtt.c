#include "core_mqtt.h"

#include <stdio.h>
#include <sys/time.h>

#include "esp_log.h"

#include "net/tls_transport.h"
#include "net/mqtt.h"
#include "define.h"

static const char *TAG = "MQTT";

#define MQTT_ENDPOINT "a1g70oszu4t891-ats.iot.eu-west-1.amazonaws.com"
#define MQTT_PORT 8883

static void Mqtt_Callback(struct MQTTContext *pContext, struct MQTTPacketInfo *pPacketInfo, struct MQTTDeserializedInfo *pDeserializedInfo);

static uint8_t mqtt_buffer[MQTT_BUFFER_SIZE];

static MQTTFixedBuffer_t mqtt_fixedBuffer = {
    .pBuffer = mqtt_buffer,
    .size = sizeof(mqtt_buffer),
};

static TransportInterface_t mqtt_transportInterface;
static MQTTContext_t mqtt_ctx = {0};
static MqttCallback mqtt_callback;

static uint32_t Mqtt_GetTimeMsFunction()
{
    struct timeval tv = {0};
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

bool Mqtt_IsConnected(void)
{
    return mqtt_ctx.connectStatus == MQTTConnected;
}

void Mqtt_ProcessLoop(void)
{
    MQTTStatus_t status = MQTT_ProcessLoop(&mqtt_ctx);

    if (status != MQTTSuccess)
    {
        ESP_LOGE(TAG, "MQTT process loop failed: %s", MQTT_Status_strerror(status));
    }
}

void Mqtt_Init(MqttCallback callback)
{
    mqtt_callback = callback;

    const char *hostname = MQTT_ENDPOINT;
    uint16_t port = MQTT_PORT;

    mqtt_transportInterface.pNetworkContext = TLSTransport_Init(hostname, port, MQTT_PROCESS_LOOP_TIMEOUT_MS, "NONE", "NONE", "NONE");

    mqtt_transportInterface.send = TLSTransport_Send;
    mqtt_transportInterface.recv = TLSTransport_Recv;

    /* initialize MQTT library */
    MQTTStatus_t status = MQTT_Init(&mqtt_ctx, &mqtt_transportInterface, Mqtt_GetTimeMsFunction, Mqtt_Callback, &mqtt_fixedBuffer);
    if (status != MQTTSuccess)
        ESP_LOGE(TAG, "init failed: %s", MQTT_Status_strerror(status));
}

void Mqtt_Connect(CString clientId, bool *sessionPresent)
{
    ESP_LOGI(TAG, "connecting to broker...");

    TLSTransport_Connect(mqtt_transportInterface.pNetworkContext);

    MQTTConnectInfo_t connectInfo = {
        .pClientIdentifier = clientId.string,
        .clientIdentifierLength = (uint16_t)clientId.length,
        .cleanSession = MQTT_ALWAYS_START_CLEAN_SESSION,
        .keepAliveSeconds = MQTT_KEEPALIVE_SECONDS,
        .pUserName = NULL,
        .userNameLength = 0,
        .pPassword = NULL,
        .passwordLength = 0,
    };

    MQTTStatus_t status = MQTT_Connect(&mqtt_ctx, &connectInfo, NULL, MQTT_CONNECT_TIMEOUT_SECONDS, sessionPresent);

    if (status != MQTTSuccess)
        ESP_LOGE(TAG, "MQTT connection failed: %s", MQTT_Status_strerror(status));
    else
        ESP_LOGI(TAG, "successfully connected to MQTT broker");
}

void Mqtt_Disconnect(bool force)
{
    ESP_LOGI(TAG, "disconnecting from MQTT broker...");

    if (!force)
    {
        MQTTStatus_t status = MQTT_Disconnect(&mqtt_ctx);
        if (status != MQTTSuccess)
            ESP_LOGE(TAG, "clean disconnect failed: %s", MQTT_Status_strerror(status));
    }

    ESP_LOGI(TAG, "closing TLS socket...");

    TLSTransport_Disconnect(mqtt_transportInterface.pNetworkContext, force);
    mqtt_ctx.connectStatus = MQTTNotConnected;
    ESP_LOGI(TAG, "now disconnected from server");
}

void Mqtt_Publish(CString topic, CBuffer data)
{
    ESP_LOGI(TAG, "publish packet");

    uint16_t packet_id = MQTT_GetPacketId(&mqtt_ctx);
    MQTTPublishInfo_t publishInfo = {
        .pTopicName = topic.string,
        .topicNameLength = (uint16_t)topic.length,
        .qos = MQTT_DEFAULT_QoS,
        .pPayload = data.buffer,
        .payloadLength = (uint16_t)data.length,
        .retain = false,
        .dup = false,
    };

    MQTTStatus_t status = MQTT_Publish(&mqtt_ctx, &publishInfo, packet_id);
    if (status != MQTTSuccess)
        ESP_LOGE(TAG, "publish failure: %s", MQTT_Status_strerror(status));
    else
        ESP_LOGI(TAG, "published packet %u", packet_id);
}

void Mqtt_Subscribe(CString topicFilter)
{
    ESP_LOGI(TAG, "subscribe to topic");

    uint16_t packet_id = MQTT_GetPacketId(&mqtt_ctx);

    MQTTSubscribeInfo_t subscribeInfo = {
        .pTopicFilter = topicFilter.string,
        .topicFilterLength = (uint16_t)topicFilter.length,
        .qos = MQTT_DEFAULT_QoS,
    };

    MQTTStatus_t status = MQTT_Subscribe(&mqtt_ctx, &subscribeInfo, 1, packet_id);
    if (status != MQTTSuccess)
        ESP_LOGE(TAG, "subscribe failure: %s", MQTT_Status_strerror(status));
    else
        ESP_LOGI(TAG, "subscribe to topic %s sent (packet id: %u)", topicFilter.string, packet_id);
}

static void Mqtt_Callback(struct MQTTContext *pContext, struct MQTTPacketInfo *pPacketInfo, struct MQTTDeserializedInfo *pDeserializedInfo)
{
    (void)pContext;

    if (pDeserializedInfo == NULL)
    {
        ESP_LOGW(TAG, "cloud: MQTT deserialize error");
        return;
    }

    if ((pPacketInfo->type & 0xF0) != MQTT_PACKET_TYPE_PUBLISH)
    {
        ESP_LOGI(TAG, "callback received [type=%u, packetId=%u]",
                 pPacketInfo->type,
                 pDeserializedInfo->packetIdentifier);
        return;
    }

    if (pDeserializedInfo->pPublishInfo == NULL)
    {
        ESP_LOGW(TAG, "cloud: MQTT deserialize error");
        return;
    }

    ESP_LOGI(TAG, "cloud: publish received [packetId=%u, topic=%.*s, payload=%zu]",
             pDeserializedInfo->packetIdentifier,
             (int)pDeserializedInfo->pPublishInfo->topicNameLength,
             pDeserializedInfo->pPublishInfo->pTopicName,
             pDeserializedInfo->pPublishInfo->payloadLength);

    CString topic = {
        .length = pDeserializedInfo->pPublishInfo->topicNameLength,
        .string = pDeserializedInfo->pPublishInfo->pTopicName,
    };

    CBuffer payload = {
        .length = pDeserializedInfo->pPublishInfo->payloadLength,
        .buffer = pDeserializedInfo->pPublishInfo->pPayload,
    };

    mqtt_callback(topic, payload);
}