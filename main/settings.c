#include "settings.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#define NVS_NAMESPACE               "nightstand"

static const char *TAG = "settings";

void settings_setup() {

    ESP_LOGI(TAG, "Starting NVS...");

    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    ESP_ERROR_CHECK(err);

    ESP_LOGI(TAG, "NVS ready.");

}

void settings_load(const char *key, uint16_t *value) {

    nvs_handle_t nvs;

    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for reading with error: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_get_u16(nvs, key, value);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded %s: %u.", key, *value);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No entry for %s, using default: %u", key, *value);
    } else {
        ESP_LOGE(TAG, "Error loading %s: %s", key, esp_err_to_name(err));
    }
    
    nvs_close(nvs);
}

void settings_save(const char *key, uint16_t value) {

    nvs_handle_t nvs;

    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing with error: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_set_u16(nvs, key, value);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Set %s to %u.", key, value);

        err = nvs_commit(nvs);

        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Saved %u to %s.", value, key);
        } else {
            ESP_LOGE(TAG, "Failed to commit %s with error: %s", key, esp_err_to_name(err));
        }

    } else {
        ESP_LOGE(TAG, "Failed to set %s with error: %s", key, esp_err_to_name(err));
    }
    
    nvs_close(nvs);
}