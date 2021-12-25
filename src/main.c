
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_log.h>

#include <esp_event.h>

#include "app_driver.h"
#include "app_wifi.h"
#include "app_mqtt.h"

static const char *TAG = "app_main";

EventGroupHandle_t wifi_event_group = NULL;

void app_main() {
    ESP_LOGI(TAG, "Smart Garden Application Started!");
    app_driver_init();

    app_wifi_start();

    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT, false, true, portMAX_DELAY);

    app_mqtt_start();
}