#ifndef APP_PUMP_H
#define APP_PUMP_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <driver/gpio.h>
#include <esp_log.h>

#include SMARTGARDEN_BOARD

void app_pump(void * pvParameters);

#endif