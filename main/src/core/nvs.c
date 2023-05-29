#include "nvs.h"
#include "nvs_flash.h"

#include "define.h"
#include "core/nvs.h"

#define NVS_NAMESPACE "storage"

static bool Nvs_GetBlob(const char *key, uint8_t **blob, size_t *length)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    ESP_ERROR_CHECK(err);

    // get size of the blob
    err = nvs_get_blob(nvs_handle, key, NULL, length);
    if (err != ESP_OK)
    {
        nvs_close(nvs_handle);
        return false;
    }

    *blob = malloc(*length);
    err = nvs_get_blob(nvs_handle, key, *blob, length);
    ESP_ERROR_CHECK(err);
    nvs_close(nvs_handle);

    return true;
}

static void Nvs_SetBlob(const char *key, const uint8_t *blob, size_t length)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    ESP_ERROR_CHECK(err);

    err = nvs_set_blob(nvs_handle, key, blob, length);
    ESP_ERROR_CHECK(err);

    err = nvs_commit(nvs_handle);
    ESP_ERROR_CHECK(err);

    nvs_close(nvs_handle);
}

void Nvs_SetString(const char *key, String string)
{
    Nvs_SetBlob(key, (uint8_t *)string.string, string.length);
}

void Nvs_GetString(const char *key, String *string)
{
    Nvs_GetBlob(key, (uint8_t **)&string->string, &string->length);
}

void Nvs_SetBuffer(const char *key, Buffer buffer)
{
    Nvs_SetBlob(key, buffer.buffer, buffer.length);
}

void Nvs_GetBuffer(const char *key, Buffer *buffer)
{
    Nvs_GetBlob(key, &buffer->buffer, &buffer->length);
}
