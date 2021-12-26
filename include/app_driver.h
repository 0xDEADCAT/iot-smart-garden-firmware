#ifndef APP_DRIVER_H
#define APP_DRIVER_H

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <iot_button.h>
#include <nvs_flash.h>
#include <esp_system.h>

#include SMARTGARDEN_BOARD

void app_driver_init(void);
int app_driver_set_state(bool state);
bool app_driver_get_state(void);

#endif