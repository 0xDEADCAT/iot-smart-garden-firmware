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
    queue_holder_t mqttQueues = *(queue_holder_t*) pvParameters;
    
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
    char buffer[16];

    while (1) {
        dht11_reading = DHT11_read();
        if (dht11_reading.status == DHT11_OK) {
            ESP_LOGI(TAG, "Temperature: %d °C", dht11_reading.temperature);
            ESP_LOGI(TAG, "Humidity: %d %%", dht11_reading.humidity);
            sprintf(buffer, "%d", dht11_reading.temperature);
            strcpy(mqtt_message.topic, "sensors/1/temperature");
            strcpy(mqtt_message.payload, buffer);
            xQueueSend(mqttQueues.outgoingQueue, &mqtt_message, portMAX_DELAY);
            sprintf(buffer, "%d", dht11_reading.humidity);
            strcpy(mqtt_message.topic, "sensors/1/humidity");
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
        strcpy(mqtt_message.topic, "sensors/1/soil_moisture");
        strcpy(mqtt_message.payload, buffer);
        xQueueSend(mqttQueues.outgoingQueue, &mqtt_message, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}