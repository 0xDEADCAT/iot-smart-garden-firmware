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
#include <freertos/event_groups.h>

#include <lwip/sockets.h>
#include <lwip/dns.h>
#include <lwip/netdb.h>

#include <mqtt_client.h>

#define MQTT_CONNECTED_EVENT    BIT0

typedef struct {
    char topic[64];
    char payload[64];
    size_t payload_len;
    size_t qos;
    size_t retain;
} mqtt_message_t;

typedef struct {
    QueueHandle_t incomingQueue;
    QueueHandle_t outgoingQueue;
} queue_holder_t;

void app_mqtt(void * pvParameters);

#endif