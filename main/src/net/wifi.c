#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

#include <string.h>

#include "net/wifi.h"
#include "core/core.h"

static const char* TAG = "Wifi";

/* FreeRTOS event group to signal when we are connected */
// static EventGroupHandle_t wifi_event_group;

/* number of times the connection was retried */
static uint32_t connection_retry_count = 0;

static void Wifi_EventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "try connect to AP SSID:%s with password:%s", ESP_WIFI_SSID, ESP_WIFI_PASS);
            ESP_ERROR_CHECK(esp_wifi_connect());
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "connection failure");
            if (connection_retry_count < ESP_MAXIMUM_RETRY)
            {
                connection_retry_count += 1;
                ESP_LOGI(TAG, "retry connect to the AP");
                esp_wifi_connect();
            }
            break;
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "connected to AP SSID: %s", ESP_WIFI_SSID);
            connection_retry_count = 0;
            break;
        default:
            ESP_LOGI(TAG, "unhandled wifi event: %d", event_id);
            break;
        }
    }

    if (event_base == IP_EVENT)
    {
        switch (event_id)
        {
        case IP_EVENT_STA_GOT_IP:
            {
                ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
                ESP_LOGI(TAG, "got ip IPv4:" IPSTR, IP2STR(&event->ip_info.ip));
                Core_EventNotify(CORE_EVENT_CONNECTED);
            }
            break;
        default:
            ESP_LOGI(TAG, "unhandled ip event: %d", event_id);
            break;
        }
    }

}

void Wifi_Init(void)
{
    ESP_LOGI(TAG, "init wifi in STA (station) mode");

    /* initialize station mode */
    esp_netif_create_default_wifi_sta();

    /* setup event handlers */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &Wifi_EventHandler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &Wifi_EventHandler,
                                                        NULL,
                                                        NULL));

    /* initi wifi driver */
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));

    /* wifi configuration */
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = ESP_WIFI_SSID,
            .password = ESP_WIFI_PASS,
	        .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void Wifi_Stop(void)
{
    ESP_LOGI(TAG, "stop wifi connection");
    ESP_ERROR_CHECK(esp_wifi_stop());
}
