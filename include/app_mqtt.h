#ifndef APP_MQTT_H
#define APP_MQTT_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <esp_wifi.h>
#include <esp_system.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_log.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

#include <lwip/sockets.h>
#include <lwip/dns.h>
#include <lwip/netdb.h>

#include <mqtt_client.h>

void app_mqtt_start(void);

#endif