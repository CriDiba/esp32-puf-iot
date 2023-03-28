#include <stdio.h>

#include "freertos/FreeRTOS.h" // portTICK_PERIOD_MS
#include "freertos/task.h" // vTaskDelay
#include "esp_system.h" // esp_restart
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"


#include "net/wifi.h"
#include "console/console.h"

void app_main(void)
{
    /* initialization */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* start wifi connection */
    Wifi_Init();

    /* wait for connection */
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    /* start tcp console */
    xTaskCreate(Console_TcpConsoleTask, "tcp-console", 4096, NULL, 5, NULL);
}
