#include "define.h"

#define MQTT_HOSTNAME = "mqtt-hostname"
#define MQTT_PORT = "mqtt-port"
#define MQTT_TLS_CLIENT_CERTIFICATE "mqtt-client-cert"
#define MQTT_TLS_BROKER_CERTIFICATE "mqtt-broker-cert"
#define MQTT_TLS_IOT_CA_CERTIFICATE "mqttiot-ca-cert"

void Nvs_SetString(const char *key, String string);
void Nvs_GetString(const char *key, String *string);
void Nvs_SetBuffer(const char *key, Buffer buffer);
void Nvs_GetBuffer(const char *key, Buffer *buffer);