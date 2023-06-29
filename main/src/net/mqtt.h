#pragma once

#include "core/error.h"
#include "define.h"

#define MQTT_BUFFER_SIZE 2048
#define MQTT_DEFAULT_QoS MQTTQoS0
#define MQTT_KEEPALIVE_SECONDS 120
#define MQTT_PROCESS_LOOP_TIMEOUT_MS 100
#define MQTT_CONNECT_TIMEOUT_SECONDS 10000
#define MQTT_ALWAYS_START_CLEAN_SESSION true

/* time to wait between one failed connect attempt to the next */
#define MQTT_CONNECT_FAILURE_INTERVAL_SEC 10

typedef void (*MqttCallback)(CString topic, CBuffer payload);

ErrorCode Mqtt_Init(MqttCallback callback);
ErrorCode Mqtt_ProcessLoop(void);
ErrorCode Mqtt_Connect(CString clientId, bool *sessionPresent, bool retry);
ErrorCode Mqtt_Disconnect(bool force);
ErrorCode Mqtt_Publish(CString topic, CBuffer data);
ErrorCode Mqtt_Subscribe(CString topicFilter);
ErrorCode Mqtt_Unsubscribe(CString topicFilter);
bool Mqtt_IsConnected(void);