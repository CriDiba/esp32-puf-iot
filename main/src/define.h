#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// MQTT
#define MQTT_ENDPOINT "a1g70oszu4t891-ats.iot.eu-west-1.amazonaws.com"
#define MQTT_PORT 8883

#define CSR_REQ_TOPIC "management/esp32-cris/csr_req"
#define CSR_RES_TOPIC "management/esp32-cris/csr_res"
#define CRT_REQ_TOPIC "management/esp32-cris/crt"
#define CRT_ACK_TOPIC "management/esp32-cris/crt_ack"
#define CRT_ERR_TOPIC "management/esp32-cris/crt_err"

// TLS CERT
#define CERT_ORGANIZATION "UNIVR"
#define DEVICE_ID "esp32-cris"
#define MGT_TOPIC_FILTER "management/esp32-cris/+"

// NVS Keys
#define NVS_DEVICE_CERT_KEY "nvs-device-cert"
#define NVS_DEVICE_CSR_KEY "nvs-device-csr"
#define NVS_DEVICE_SALT_KEY "ecc-salt"

#define NVS_DEVICE_CERT_KEY_TMP "nvs-device-cert-tmp"
#define NVS_DEVICE_CSR_KEY_TMP "nvs-device-csr-tmp"
#define NVS_DEVICE_SALT_KEY_TMP "ecc-salt-tmp"

#define STR_CHALL_MAX_LEN 20

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define NEWLINE "\r\n"

typedef struct
{
    size_t length;
    char *string;
} String;

typedef struct
{
    size_t length;
    const char *string;
} CString;

#define mkCSTRING(s) ((CString){.string = (s), .length = (sizeof(s) - 1)})

typedef struct
{
    size_t length;
    uint8_t *buffer;
} Buffer;

typedef struct
{
    size_t length;
    const uint8_t *buffer;
} CBuffer;