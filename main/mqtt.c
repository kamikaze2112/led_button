#include "mqtt.h"
#include <stdbool.h>
#include "esp_log.h"
#include "esp_event.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "mqtt_client.h"
#include "secrets.h"
#include "esp_mac.h"
#include <stdio.h>
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>
#include "led.h"

static const char *TAG = "mqtt";

static char device_uid[16];
static char status_topic[32];
static char min_set[48];
static char max_set[48];
static char fade_set[48];
static char min_state[48];
static char max_state[48];
static char fade_state[48];

static esp_mqtt_client_handle_t mqtt_handle;

static bool client_started = false;

static char press_topic[40];

static void mqtt_publish(const char *topic, const char *payload, int qos, int retain) {

    int msg_id = esp_mqtt_client_publish(mqtt_handle, topic, payload, 0, qos, retain);

    if (msg_id == -1) {
        ESP_LOGE(TAG, "Publish failed");
    } else if (msg_id == -2) {
        ESP_LOGE(TAG, "Publish failed, outbox full.");
    } else {
        ESP_LOGI(TAG, "Publish Success: %d", msg_id);
    }

}

static void mqtt_state() {

    char v[8];
    snprintf(v, sizeof(v), "%d", led_get_min());
    mqtt_publish(min_state, v, 1, 1);
    snprintf(v, sizeof(v), "%d", led_get_max());
    mqtt_publish(max_state, v, 1, 1);
    snprintf(v, sizeof(v), "%d", led_get_fade());
    mqtt_publish(fade_state, v, 1, 1);

}

static void mqtt_data(int topic_len, const char *topic, int data_len, const char *data) {

    char d[10];
    snprintf(d, sizeof(d), "%.*s", data_len, data);
    int dnum = atoi(d);

    if (dnum < 0) dnum = 0;

    if (topic_len == strlen(min_set) && strncmp(topic, min_set, topic_len) == 0) {
        led_set_min(dnum);
    }

    if (topic_len == strlen(max_set) && strncmp(topic, max_set, topic_len) == 0) {
        led_set_max(dnum);
    }

    if (topic_len == strlen(fade_set) && strncmp(topic, fade_set, topic_len) == 0) {
        led_set_fade(dnum);
    }

    mqtt_state();
}

static void mqtt_subscribe(const char *topic) {

    int msg_id = esp_mqtt_client_subscribe_single(mqtt_handle, topic, 1);

    if (msg_id == -1) {
        ESP_LOGE(TAG, "Subscribe to %s failed", topic);
    }

}

static void mqtt_discovery() {

    char press_id[40];
    char config_topic[64];
    char min_id[40];
    char max_id[40];
    char fade_id[40];
      
    snprintf(press_id, sizeof(press_id), "%s_press", device_uid);
    snprintf(config_topic, sizeof(config_topic), "homeassistant/device/%s/config", device_uid);
    snprintf(min_id, sizeof(min_id), "%s_led_min", device_uid);
    snprintf(max_id, sizeof(max_id), "%s_led_max", device_uid);
    snprintf(fade_id, sizeof(fade_id), "%s_fade_time", device_uid);


    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "availability_topic", status_topic);
    cJSON *dev = cJSON_AddObjectToObject(root, "device");
    cJSON_AddStringToObject(dev, "identifiers", device_uid);
    cJSON_AddStringToObject(dev, "name", device_uid);
    cJSON *origin = cJSON_AddObjectToObject(root, "origin");
    cJSON_AddStringToObject(origin, "name", "button-fw");

    cJSON *comps = cJSON_AddObjectToObject(root, "components");
    cJSON *press = cJSON_AddObjectToObject(comps, "press");
    cJSON_AddStringToObject(press, "platform", "event");
    cJSON_AddStringToObject(press, "name", "Press");
    cJSON_AddStringToObject(press, "unique_id", press_id);
    cJSON_AddStringToObject(press, "state_topic", press_topic);
    
    cJSON *types = cJSON_AddArrayToObject(press, "event_types");
    cJSON_AddItemToArray(types, cJSON_CreateString("short_press"));
    cJSON_AddItemToArray(types, cJSON_CreateString("long_press"));
    cJSON_AddItemToArray(types, cJSON_CreateString("longer_press"));

    cJSON *led_min = cJSON_AddObjectToObject(comps, "led_min");
    cJSON_AddStringToObject(led_min, "platform", "number");
    cJSON_AddStringToObject(led_min, "name", "LED PWM Lower");
    cJSON_AddStringToObject(led_min, "unique_id", min_id);
    cJSON_AddStringToObject(led_min, "state_topic", min_state);
    cJSON_AddStringToObject(led_min, "command_topic", min_set);
    cJSON_AddNumberToObject(led_min, "min", 0);
    cJSON_AddNumberToObject(led_min, "max", 100);
    cJSON_AddNumberToObject(led_min, "step", 1);
    cJSON_AddStringToObject(led_min, "mode", "slider");

    cJSON *led_max = cJSON_AddObjectToObject(comps, "led_max");
    cJSON_AddStringToObject(led_max, "platform", "number");
    cJSON_AddStringToObject(led_max, "name", "LED PWM Upper");
    cJSON_AddStringToObject(led_max, "unique_id", max_id);
    cJSON_AddStringToObject(led_max, "state_topic", max_state);
    cJSON_AddStringToObject(led_max, "command_topic", max_set);
    cJSON_AddNumberToObject(led_max, "min", 0);
    cJSON_AddNumberToObject(led_max, "max", 100);
    cJSON_AddNumberToObject(led_max, "step", 1);
    cJSON_AddStringToObject(led_max, "mode", "slider");

    cJSON *fade_ms = cJSON_AddObjectToObject(comps, "fade_ms");
    cJSON_AddStringToObject(fade_ms, "platform", "number");
    cJSON_AddStringToObject(fade_ms, "name", "Fade Time (ms)");
    cJSON_AddStringToObject(fade_ms, "unique_id", fade_id);
    cJSON_AddStringToObject(fade_ms, "state_topic", fade_state);
    cJSON_AddStringToObject(fade_ms, "command_topic", fade_set);
    cJSON_AddNumberToObject(fade_ms, "min", 0);
    cJSON_AddNumberToObject(fade_ms, "max", 5000);
    cJSON_AddNumberToObject(fade_ms, "step", 1);
    cJSON_AddStringToObject(fade_ms, "mode", "slider");

    char *payload = cJSON_Print(root);
    ESP_LOGI(TAG, "%s", payload);

    mqtt_publish(config_topic, payload, 1, 1);

    cJSON_free(payload);
    cJSON_Delete(root);
    
}

static void mqtt_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {

    switch (event_id)
    {
    case MQTT_EVENT_CONNECTED:

        {
            ESP_LOGI(TAG, "MQTT Connected");
            mqtt_publish(status_topic, "online", 1, 1);
            mqtt_discovery();
            mqtt_state();
            mqtt_subscribe(min_set);
            mqtt_subscribe(max_set);
            
        }
        break;

    case MQTT_EVENT_DISCONNECTED:
        
        ESP_LOGI(TAG, "MQTT Disconnected");
        break;

    case MQTT_EVENT_DATA: 

        {
            esp_mqtt_event_handle_t event = event_data;
            ESP_LOGI(TAG, "got %.*s = %.*s", event->topic_len, event->topic, event->data_len, event->data);
            mqtt_data(event->topic_len, event->topic, event->data_len, event->data);

        }
        break;

    default:
        break;
    }

}

static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {

    if (client_started){
        return;
    } else {

        esp_err_t err = esp_mqtt_client_start(mqtt_handle);

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "MQTT Client failed with error: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "MQTT Client started.");
            client_started = true;
        }
    }
}

void mqtt_setup() {

    ESP_LOGI(TAG, "MQTT setup starting...");
    
    uint8_t device_mac[6];

    ESP_ERROR_CHECK(esp_read_mac(device_mac, ESP_MAC_WIFI_STA));

    snprintf(device_uid, sizeof(device_uid), "button_%02x%02x%02x", device_mac[3], device_mac[4], device_mac[5]);

    snprintf(status_topic, sizeof(status_topic), "%s/status", device_uid);
    snprintf(press_topic, sizeof(press_topic), "%s/button", device_uid);
    snprintf(min_set, sizeof(min_set), "%s/led_min/set", device_uid);
    snprintf(max_set, sizeof(max_set), "%s/led_max/set", device_uid);
    snprintf(fade_set, sizeof(fade_set), "%s/fade/set", device_uid);
    snprintf(min_state, sizeof(min_state), "%s/led_min/state", device_uid);
    snprintf(max_state, sizeof(max_state), "%s/led_max/state", device_uid);
    snprintf(fade_state, sizeof(fade_state), "%s/fade/state", device_uid);

    ESP_LOGW(TAG, "Device_UID: %s", device_uid);
    ESP_LOGW(TAG, "Status Topic: %s", status_topic);

    esp_mqtt_client_config_t client_config = {
        .broker = {
            .address = {
                .uri = MQTT_URI,
            },
        },
        .session = {
            .keepalive = 30,
            .last_will = {
                .topic = status_topic,
                .msg = "offline",
                .qos = 1,
                .retain = 1,
            },
        },
        .credentials = {
            .username = MQTT_USER,
            .authentication = {
                .password = MQTT_PASS,
            },
        },
    };

    mqtt_handle = esp_mqtt_client_init(&client_config);

    if (mqtt_handle == NULL) {
        ESP_LOGE(TAG, "MQTT Handle returned NULL");
        return;
    }

    ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt_handle, MQTT_EVENT_ANY, mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, ip_event_handler, NULL, NULL));
     
}   

void mqtt_button(press_t press) {

    const char *payload = NULL;
    
    switch (press) {

        case MQTT_SHORT_PRESS:
            payload = "{\"event_type\":\"short_press\"}";
        break;

        case MQTT_LONG_PRESS:
            payload = "{\"event_type\":\"long_press\"}";
        break;

        case MQTT_LONGER_PRESS:
            payload = "{\"event_type\":\"longer_press\"}";
        break;

        default:
        break;
    }

        if (payload == NULL) {
            ESP_LOGE(TAG, "NULL Payload, returning...");
            return;
        }

        mqtt_publish(press_topic, payload, 0, 0);
 
}