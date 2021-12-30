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

void app_sensor(void * pvParameters) {
    QueueHandle_t mqttQueue = (QueueHandle_t) pvParameters;
    
    // Initialize DHT sensor.
    DHT11_init(GPIO_NUM_22);

    // Configure adc
    adc1_config_width(width);
    adc1_config_channel_atten(channel, atten);

    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, width, DEFAULT_VREF, adc_chars);
    print_char_val_type(val_type);

    while (1) {
        dht11_reading = DHT11_read();
        if (dht11_reading.status == DHT11_OK) {
            ESP_LOGI(TAG, "Temperature: %d °C", dht11_reading.temperature);
            ESP_LOGI(TAG, "Humidity: %d %%", dht11_reading.humidity);
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
        ESP_LOGI(TAG, "Soil Moisture - Raw: %d\tVoltage: %dmV", adc_reading, voltage);
        xQueueSend(mqttQueue, (void*)&adc_reading, 0);
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}