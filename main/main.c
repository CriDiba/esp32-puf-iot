#include <stdio.h>

#include "freertos/FreeRTOS.h" // portTICK_PERIOD_MS
#include "freertos/task.h" // vTaskDelay
#include "esp_system.h" // esp_restart

void app_main(void)
{
    printf("Hello world!\n");

    for (int i = 10; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}