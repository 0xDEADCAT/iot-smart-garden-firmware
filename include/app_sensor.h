#ifndef APP_SENSOR_H
#define APP_SENSOR_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <driver/gpio.h>
#include <driver/adc.h>
#include <esp_log.h>
#include <esp_adc_cal.h>
#include <nvs_flash.h>
#include <dht11.h>

#include "app_mqtt.h"

#include SMARTGARDEN_BOARD

#define FCTRY_PARTITION_NAME "fctry"

void app_sensor(void * pvParameters);

#endif