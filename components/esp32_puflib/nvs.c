//
// Ondrej Stanicek
// staniond@fit.cvut.cz
// Czech Technical University - Faculty of Information Technology
// 2022
//
#include <stdbool.h>
#include <stdlib.h>
#include <esp_err.h>
#include <nvs_flash.h>
#include "nvs.h"

#define DEVICE_NAME_KEY "DEVICE_NAME"
#define NVS_NAMESPACE "storage"

/**
 * Initializes the NVS subsystem.
 * @param open_mode NVS_READWRITE or NVS_READONLY
 * @param handle pointer to nvs handle to set
 * @return NVS error state
 */
esp_err_t initialize_nvs(nvs_open_mode_t open_mode, nvs_handle_t* handle) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    return nvs_open(NVS_NAMESPACE, open_mode, handle);
}

bool get_blob(uint8_t **blob, size_t *length, const char *key) {
    nvs_handle_t my_handle;
    esp_err_t err = initialize_nvs(NVS_READONLY, &my_handle);
    ESP_ERROR_CHECK( err );

    // get size of the blob
    err = nvs_get_blob(my_handle, key, NULL, length);
    if(err != ESP_OK) {
        nvs_close(my_handle);
        return false;
    }

    *blob = malloc(*length);
    err = nvs_get_blob(my_handle, key, *blob, length);
    ESP_ERROR_CHECK( err );
    nvs_close(my_handle);

    return true;
}

void set_blob(const uint8_t *blob, size_t length, const char *key) {
    nvs_handle_t my_handle;
    esp_err_t err = initialize_nvs(NVS_READWRITE, &my_handle);
    ESP_ERROR_CHECK( err );

    err = nvs_set_blob(my_handle, key, blob, length);
    ESP_ERROR_CHECK( err );

    err = nvs_commit(my_handle);
    ESP_ERROR_CHECK( err );

    nvs_close(my_handle);
}
