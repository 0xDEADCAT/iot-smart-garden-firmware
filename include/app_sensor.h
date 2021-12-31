#ifndef APP_SENSOR_H
#define APP_SENSOR_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <driver/gpio.h>
#include <driver/adc.h>
#include <esp_log.h>
#include <esp_adc_cal.h>
#include <dht11.h>

#include "app_mqtt.h"

#include SMARTGARDEN_BOARD

void app_sensor(void * pvParameters);

#endif