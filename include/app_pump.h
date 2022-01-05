#ifndef APP_PUMP_H
#define APP_PUMP_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include "app_mqtt.h"

#include SMARTGARDEN_BOARD

#define FCTRY_PARTITION_NAME "fctry"

void app_pump(void * pvParameters);

#endif