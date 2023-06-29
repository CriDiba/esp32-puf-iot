#include "esp_err.h"   // esp_err_t
#include "nvs_flash.h" // nvs_flash_init
#include "esp_netif.h" // esp_netif_init
#include "esp_event.h" // esp_event_loop_create_default
#include "esp_sleep.h" // esp_default_wake_deep_sleep

#include "core/core.h" // Core_TaskStart

#include "puflib.h" // puflib_init

#include "crypto/crypto.h"

void app_main(void)
{
    /* puflib handler */
    puflib_init();

    /* initialization */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* start main task */
    Core_TaskStart();
}

void RTC_IRAM_ATTR esp_wake_deep_sleep(void)
{
    esp_default_wake_deep_sleep();
    /* call puflib wake up to copy SRAM value in buffer */
    puflib_wake_up_stub();
}