#include "app_mqtt.h"

static const char *TAG = "app_mqtt";

EventGroupHandle_t mqtt_event_group = NULL;
static queue_holder_t mqttQueues;
static char *device_id;
#if CONFIG_DEVICE_SENSOR
static TaskHandle_t sensor_task_handle = NULL;
int sleep_time_msg_id = -1;
#endif

static void log_error_if_nonzero(const char * message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            xEventGroupSetBits(mqtt_event_group, MQTT_CONNECTED_EVENT);

#if CONFIG_DEVICE_PUMP
            char topic[32];
            sprintf(topic, "pumps/%s/activate", device_id);
            msg_id = esp_mqtt_client_subscribe(client, topic, 2);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
#elif CONFIG_DEVICE_SENSOR
            char topic[32];
            sprintf(topic, "sensors/%s/sleep_time", device_id);
            msg_id = esp_mqtt_client_subscribe(client, topic, 2);
            sleep_time_msg_id = msg_id;
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
#endif
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            xEventGroupClearBits(mqtt_event_group, MQTT_CONNECTED_EVENT);
            break;
        case MQTT_EVENT_SUBSCRIBED:
#if CONFIG_DEVICE_SENSOR
            if (event->msg_id == sleep_time_msg_id) {
                xTaskNotify(sensor_task_handle, 0, eNoAction);
            }
#endif
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
#if CONFIG_DEVICE_SENSOR
            xTaskNotify(sensor_task_handle, 0, eIncrement);
#endif
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            mqtt_message_t message = {
                .payload_len = event->data_len,
                .retain = event->retain
            };
            strncpy(message.topic, event->topic, event->topic_len);
            strncpy(message.payload, event->data, event->data_len);
            xQueueSend(mqttQueues.incomingQueue, &message, portMAX_DELAY);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
                log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
                log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
                ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

            }
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

void app_mqtt(void * pvParameters)
{
    // Get device ID from fctry partition
    nvs_handle fctry_handle;
    if (nvs_flash_init_partition(FCTRY_PARTITION_NAME) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init %s NVS partition", FCTRY_PARTITION_NAME);
        return;
    }
    if (nvs_open_from_partition(FCTRY_PARTITION_NAME, "fctry_ns", NVS_READWRITE, &fctry_handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open %s NVS partition", FCTRY_PARTITION_NAME);
        return;
    }
    size_t device_id_len;

    if (nvs_get_str(fctry_handle, "device_id", NULL, &device_id_len) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get device_id length from %s NVS partition", FCTRY_PARTITION_NAME);
        return;
    }
    device_id = malloc(device_id_len);
    if (nvs_get_str(fctry_handle, "device_id", device_id, &device_id_len) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get device_id from %s NVS partition", FCTRY_PARTITION_NAME);
        return;
    }

#if CONFIG_DEVICE_SENSOR
    sensor_task_handle = xTaskGetHandle("app_sensor");

    configASSERT(sensor_task_handle);

    xTaskNotifyStateClear(NULL);
#endif

    mqttQueues = *(queue_holder_t*) pvParameters;
    mqtt_event_group = xEventGroupCreate();

    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_MQTT_BROKER_URL,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);

    mqtt_message_t message;

    xEventGroupWaitBits(mqtt_event_group, MQTT_CONNECTED_EVENT, false, true, portMAX_DELAY);
    while(1) {
#if CONFIG_DEVICE_SENSOR
        if (xTaskNotifyWait(0, 0, NULL, 0) == pdTRUE) {
            ESP_LOGI(TAG, "Got notification, exiting loop.");
            esp_mqtt_client_disconnect(client);
            esp_mqtt_client_stop(client);
            xTaskNotify(sensor_task_handle, 0, eIncrement);
            vTaskDelete(NULL);
        }
#endif
        if(xQueueReceive(mqttQueues.outgoingQueue, &message, 0) == pdPASS) {
            ESP_LOGI(TAG, "Publishing on topic: %s -> message: %s", message.topic, message.payload);
            esp_mqtt_client_publish(client, message.topic, message.payload, message.payload_len, message.qos, message.retain);
        }
    }
}