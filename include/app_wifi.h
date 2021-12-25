#ifndef APP_WIFI_H
#define APP_WIFI_H

#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_log.h>

#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_ble.h>

#include <nvs_flash.h>

#define WIFI_CONNECTED_EVENT BIT0

/* Signal Wi-Fi events on this event-group */
extern EventGroupHandle_t wifi_event_group;

void app_wifi_start(void);

#endif