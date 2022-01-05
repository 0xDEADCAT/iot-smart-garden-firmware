#include "app_pump.h"

static const char *TAG = "app_pump";

static char *device_id;

static bool g_pump_state = false;

// Pump reset timer handler
static TimerHandle_t pump_reset_timer = NULL;
static TickType_t pump_reset_timer_period = CONFIG_PUMP_RESET_TIMER_PERIOD / portTICK_PERIOD_MS;

queue_holder_t mqttQueues;

// get the current state of the pump
bool app_pump_get_state(void)
{
    return g_pump_state;
}

static void set_pump_state(bool target)
{
    gpio_set_level(SMARTGARDEN_BOARD_PUMP_GPIO, target);
}

// set the state of the pump
int IRAM_ATTR app_pump_set_state(bool state)
{
    if(g_pump_state != state) {
        mqtt_message_t mqtt_message = {
            .payload_len = 0,
            .qos = 2,
            .retain = 0
        };
        char topic[32];
        sprintf(topic, "pumps/%s/state", device_id);
        strcpy(mqtt_message.topic, topic);
        g_pump_state = state;
        if (g_pump_state == true) {
            strcpy(mqtt_message.payload, "ON");
            xTimerStart(pump_reset_timer, 0);
        } else {
            strcpy(mqtt_message.payload, "OFF");
            xTimerStop(pump_reset_timer, 0);
        }
        set_pump_state(g_pump_state);
        xQueueSend(mqttQueues.outgoingQueue, &mqtt_message, portMAX_DELAY);
    }
    return ESP_OK;
}

static void pump_reset_timer_callback(TimerHandle_t xTimer) {
    ESP_LOGI(TAG, "Pump reset timer expired");
    app_pump_set_state(false);
}

void app_pump(void * pvParameters) {
    ESP_LOGI(TAG, "Starting pump task...");

    mqttQueues = *(queue_holder_t*) pvParameters;

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

    // Initialize the pump reset timer
    pump_reset_timer = xTimerCreate(
        "Pump Reset Timer",
        pump_reset_timer_period,
        pdFALSE,
        ( void * ) 0,
        pump_reset_timer_callback);

    while(1) {
        if(xQueueReceive(mqttQueues.incomingQueue, &message, portMAX_DELAY)) {
            ESP_LOGI(TAG, "Received message - %s: %s", message.topic, message.payload);
            sprintf(topic, "pumps/%s/activate", device_id);
            if(strcmp(message.topic, topic) == 0) {
                if(strcmp(message.payload, "true") == 0) {
                    app_pump_set_state(true);
                } 
                else if(strcmp(message.payload, "false") == 0) {
                    app_pump_set_state(false);
                }
            }
        }
    }
}