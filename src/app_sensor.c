#include "app_sensor.h"

#define DEFAULT_VREF    1100
#define NO_OF_SAMPLES   64

static const char *TAG = "app_sensor";

static esp_adc_cal_characteristics_t *adc_chars;
static const adc_channel_t channel = ADC_CHANNEL_4;
static const adc_bits_width_t width = ADC_WIDTH_BIT_12;
static const adc_atten_t atten = ADC_ATTEN_DB_11;
static const adc_unit_t unit = ADC_UNIT_1;
static struct dht11_reading dht11_reading;
static char *device_id;

static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        ESP_LOGI(TAG, "Characterized using Two Point Value");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        ESP_LOGI(TAG, "Characterized using eFuse Vref");
    } else {
        ESP_LOGI(TAG, "Characterized using Default Vref");
    }
}

static float map_and_constrain(float x, float in_min, float in_max, float out_min, float out_max)
{
    float mapped_val = (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    if (mapped_val > out_max) {
        mapped_val = out_max;
    } else if (mapped_val < out_min) {
        mapped_val = out_min;
    }
    return mapped_val;
}

void app_sensor(void * pvParameters) {
    xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);

    ESP_LOGI(TAG, "Starting sensor task...");

    queue_holder_t mqttQueues = *(queue_holder_t*) pvParameters;

    // Initialize and open fctry partition
    nvs_handle fctry_handle;
    if (nvs_flash_init_partition(FCTRY_PARTITION_NAME) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init %s NVS partition", FCTRY_PARTITION_NAME);
        return;
    }
    if (nvs_open_from_partition(FCTRY_PARTITION_NAME, "fctry_ns", NVS_READWRITE, &fctry_handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open %s NVS partition", FCTRY_PARTITION_NAME);
        return;
    }

    // Get device ID from fctry partition
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

    mqtt_message_t incoming_mqtt_message;

    uint32_t deepsleep_time;
    bool received_deepsleep_time = false;

    // Open default NVS
    nvs_handle sensor_nvs_handle;
    ESP_ERROR_CHECK(nvs_open("sensor", NVS_READWRITE, &sensor_nvs_handle));

    char buffer[16], topic[32];
    sprintf(topic, "sensors/%s/sleep_time", device_id);

    if (xQueueReceive(mqttQueues.incomingQueue, &incoming_mqtt_message, 500 / portTICK_PERIOD_MS) == pdTRUE && incoming_mqtt_message.topic != NULL) {
        if (strcmp(incoming_mqtt_message.topic, topic) == 0) {
            deepsleep_time = atoi(incoming_mqtt_message.payload);
            ESP_LOGI(TAG, "Received sleep time: %d", deepsleep_time);
            received_deepsleep_time = true;
            // Store deepsleep time in NVS
            ESP_ERROR_CHECK(nvs_set_u32(sensor_nvs_handle, "sleep_time", deepsleep_time));
            ESP_ERROR_CHECK(nvs_commit(sensor_nvs_handle));
            // Remove retained sleep time message from MQTT broker
            mqtt_message_t outgoing_mqtt_message = {
                .payload = "",
                .payload_len = 0,
                .qos = 2,
                .retain = true
            };
            strcpy(outgoing_mqtt_message.topic, topic);
            xQueueSend(mqttQueues.outgoingQueue, &outgoing_mqtt_message, 0);
        }
    }
    if (received_deepsleep_time == false) {
        ESP_LOGI(TAG, "No sleep time received, trying to get value from NVS");
        if (nvs_get_u32(sensor_nvs_handle, "sleep_time", &deepsleep_time) == ESP_OK) {
            ESP_LOGI(TAG, "Found sleep time in NVS: %d", deepsleep_time);
        } else {
            ESP_LOGI(TAG, "No sleep time found in NVS, using default value: %d", CONFIG_SENSOR_DEEPSLEEP_TIME);
            deepsleep_time = CONFIG_SENSOR_DEEPSLEEP_TIME;
        }
    }
    nvs_close(sensor_nvs_handle);

    ESP_LOGI(TAG, "!!! SLEEP WAKE UP CAUSE: %d !!!", esp_sleep_get_wakeup_cause());

    // Setup deep sleep
    esp_sleep_enable_timer_wakeup(deepsleep_time * uS_TO_S_FACTOR);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
    
    // Initialize DHT sensor.
    DHT11_init(GPIO_NUM_22);

    // Configure adc
    adc1_config_width(width);
    adc1_config_channel_atten(channel, atten);

    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, width, DEFAULT_VREF, adc_chars);
    print_char_val_type(val_type);

    float soil_moisture_percent = 0;
    mqtt_message_t mqtt_message = {
        .payload_len = 0,
        .qos = 2,
        .retain = 0
    };

    uint32_t notif_val = 0;

    TaskHandle_t mqtt_task_handle = xTaskGetHandle("app_mqtt");

    configASSERT(mqtt_task_handle);

    xTaskNotifyStateClear(NULL);

    do {
        dht11_reading = DHT11_read();
    } while (dht11_reading.status != DHT11_OK);
    if (dht11_reading.status == DHT11_OK) {
        ESP_LOGI(TAG, "Temperature: %d °C", dht11_reading.temperature);
        ESP_LOGI(TAG, "Humidity: %d %%", dht11_reading.humidity);
        sprintf(buffer, "%d", dht11_reading.temperature);
        sprintf(topic, "sensors/%s/temperature", device_id);
        strcpy(mqtt_message.topic, topic);
        strcpy(mqtt_message.payload, buffer);
        xQueueSend(mqttQueues.outgoingQueue, &mqtt_message, portMAX_DELAY);
        sprintf(buffer, "%d", dht11_reading.humidity);
        sprintf(topic, "sensors/%s/humidity", device_id);
        strcpy(mqtt_message.topic, topic);
        strcpy(mqtt_message.payload, buffer);
        xQueueSend(mqttQueues.outgoingQueue, &mqtt_message, portMAX_DELAY);
    } else {
        ESP_LOGE(TAG, "DHT11 read error: %s", (dht11_reading.status == DHT11_CRC_ERROR) ? "CRC Error" : "Timeout");
    }
    
    uint32_t adc_reading = 0;
    
    // Multisampling
    for (int i = 0; i < NO_OF_SAMPLES; i++) {
        adc_reading += adc1_get_raw((adc1_channel_t)channel);
    }
    adc_reading /= NO_OF_SAMPLES;
    // Convert adc_reading to voltage in mV
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
    soil_moisture_percent = map_and_constrain(adc_reading, 3400, 1300, 0, 100);
    ESP_LOGI(TAG, "Soil Moisture: %f %%", soil_moisture_percent);
    ESP_LOGI(TAG, "Soil Moisture - Raw: %d\tVoltage: %dmV", adc_reading, voltage);
    sprintf(buffer, "%.2f", soil_moisture_percent);
    sprintf(topic, "sensors/%s/soil_moisture", device_id);
    strcpy(mqtt_message.topic, topic);
    strcpy(mqtt_message.payload, buffer);
    xQueueSend(mqttQueues.outgoingQueue, &mqtt_message, portMAX_DELAY);
    while (notif_val < 3) {
        xTaskNotifyWait(0, 0, &notif_val, 100 / portTICK_PERIOD_MS);
    }
    xTaskNotifyStateClear(NULL);
    xTaskNotify(mqtt_task_handle, 0, eNoAction);
    xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);
    esp_wifi_stop();
    esp_deep_sleep_start();
}