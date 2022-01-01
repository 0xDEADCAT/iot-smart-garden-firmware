#include "app_pump.h"

static const char *TAG = "app_pump";

// get the current state of the pump
bool app_pump_get_state(void)
{
    return gpio_get_level(SMARTGARDEN_BOARD_PUMP_GPIO);
}

// set the state of the pump
int app_pump_set_state(bool state)
{
    gpio_set_level(SMARTGARDEN_BOARD_PUMP_GPIO, state);
    return ESP_OK;
}

void app_pump(void * pvParameters) {
    queue_holder_t mqttQueues = *(queue_holder_t*) pvParameters;

    // Configure relay GPIO
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 1,
    };
    io_conf.pin_bit_mask = ((uint64_t)1 << SMARTGARDEN_BOARD_PUMP_GPIO);
    /* Configure the GPIO */
    gpio_config(&io_conf);

    mqtt_message_t message;

    while(1) {
        if(xQueueReceive(mqttQueues.incomingQueue, &message, portMAX_DELAY)) {
            ESP_LOGI(TAG, "Received message - %s: %s", message.topic, message.payload);
            if(strcmp(message.topic, "pumps/1/state") == 0) {
                if(strcmp(message.payload, "ON") == 0) {
                    app_pump_set_state(true);
                } else if(strcmp(message.payload, "OFF") == 0) {
                    app_pump_set_state(false);
                }
            }
        }
    }
}