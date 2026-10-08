#include "mqtt.h"
#include <stdbool.h>
#include "esp_log.h"
#include "esp_event.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "mqtt_client.h"
#include "secrets.h"

static const char *TAG = "mqtt";

static esp_mqtt_client_handle_t mqtt_handle;

static bool client_started = false;

static void mqtt_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {

    switch (event_id)
    {
    case MQTT_EVENT_CONNECTED:

        {
            
            ESP_LOGI(TAG, "MQTT Connected");

            int msg_id = esp_mqtt_client_publish(mqtt_handle, "test/c6", "Hello MQTT!", 0, 1, 0);

            if (msg_id == -1) {
                ESP_LOGE(TAG, "Publish failed");
            } else if (msg_id == -2) {
                ESP_LOGE(TAG, "Publish failed, outbox full.");
            } else {
                ESP_LOGI(TAG, "Publish Success: %d", msg_id);
            }
        }
        break;

    case MQTT_EVENT_DISCONNECTED:
        
        ESP_LOGI(TAG, "MQTT Disconnected");
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

    esp_mqtt_client_config_t client_config = {
        .broker = {
            .address = {
                .uri = MQTT_URI,
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