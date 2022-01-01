#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_log.h>
#include <esp_event.h>

#include "app_driver.h"
#include "app_wifi.h"
#include "app_mqtt.h"
#if CONFIG_DEVICE_SENSOR
#include "app_sensor.h"
#elif CONFIG_DEVICE_PUMP
#include "app_pump.h"
#endif

static const char *TAG = "app_main";

EventGroupHandle_t wifi_event_group = NULL;
static queue_holder_t mqttQueues;

void app_main() {
    ESP_LOGI(TAG, "Smart Garden Application Started!");
    app_driver_init();

    app_wifi_start();

    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT, false, true, portMAX_DELAY);

    mqttQueues.incomingQueue = xQueueCreate(10, sizeof(mqtt_message_t));
    mqttQueues.outgoingQueue = xQueueCreate(10, sizeof(mqtt_message_t));

    xTaskCreate(app_mqtt,
                "app_mqtt",
                8192,
                (void *) &mqttQueues,
                1,
                NULL);

#if CONFIG_DEVICE_SENSOR
    xTaskCreate(app_sensor,
                "app_sensor",
                8192,
                (void *) &mqttQueues,
                1,
                NULL);
#elif CONFIG_DEVICE_PUMP
    xTaskCreate(app_pump,
                "app_pump",
                8192,
                (void *) &mqttQueues,
                1,
                NULL);
#else
    #error "Device type not specified in makeconfig!"
#endif
}