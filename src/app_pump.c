#include "app_pump.h"

static const char *TAG = "app_pump";

static char *device_id;

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
    ESP_LOGI(TAG, "Starting pump task...");

    queue_holder_t mqttQueues = *(queue_holder_t*) pvParameters;

    // Get device ID from fctry partition
    nvs_handle fctry_handle;
    if (nvs_flash_init_partition(FCTRY_PARTITION_NAME) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init %s NVS partition", FCTRY_PARTITION_NAME);
        return;
    }
    if (nvs_open_from_partition(FCTRY_PARTITION_NAME, "fctry_ns", NVS_READWRITE, &fctry_handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open %s NVS partition", FCTRY_PARTITION_NAME);
        return;
    }
    size_t device_id_len;

    if (nvs_get_str(fctry_handle, "device_id", NULL, &device_id_len) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get device_id length from %s NVS partition", FCTRY_PARTITION_NAME);
        return;
    }
    device_id = malloc(device_id_len);
    if (nvs_get_str(fctry_handle, "device_id", device_id, &device_id_len) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get device_id from %s NVS partition", FCTRY_PARTITION_NAME);
        return;
    }

    // Configure relay GPIO
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 1,
    };
    io_conf.pin_bit_mask = ((uint64_t)1 << SMARTGARDEN_BOARD_PUMP_GPIO);
    /* Configure the GPIO */
    gpio_config(&io_conf);

    mqtt_message_t message;
    char topic[32];


    while(1) {
        if(xQueueReceive(mqttQueues.incomingQueue, &message, portMAX_DELAY)) {
            ESP_LOGI(TAG, "Received message - %s: %s", message.topic, message.payload);
            sprintf(topic, "pumps/%s/state", device_id);
            if(strcmp(message.topic, topic) == 0) {
                if(strcmp(message.payload, "ON") == 0) {
                    app_pump_set_state(true);
                } else if(strcmp(message.payload, "OFF") == 0) {
                    app_pump_set_state(false);
                }
            }
        }
    }
}