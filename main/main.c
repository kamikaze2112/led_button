#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/queue.h"
#include "driver/ledc.h"
#include "nvs.h"
#include "nvs_flash.h"

#define NVS_NAMESPACE               "nightstand"
#define NVS_KEY_DUTY_MIN            "led_min"
#define NVS_KEY_DUTY_MAX            "led_max"
#define NVS_KEY_FADE_TIME_MS        "fade_time"

#define VIBE_GPIO                   GPIO_NUM_3
#define LED_GPIO                    GPIO_NUM_2
#define BUTTON_GPIO                 GPIO_NUM_1

#define LONG_PRESS_MS               1000
#define LONGER_PRESS_MS             2500

#define VIBE_PULSE_ON               200
#define VIBE_PULSE_OFF              100

#define LEDC_FREQ_HZ                4000

#define LED_DUTY_MIN_DEFAULT        10
#define LED_DUTY_MAX_DEFAULT        4096
#define LED_FADE_TIME_MS_DEFAULT    2500


static const char *TAG = "nightstand_button";
static QueueHandle_t vibeQueue;

static uint16_t led_duty_min = LED_DUTY_MIN_DEFAULT;
static uint16_t led_duty_max = LED_DUTY_MAX_DEFAULT;
static uint16_t led_fade_time_ms = LED_FADE_TIME_MS_DEFAULT;

static void load_setting(nvs_handle_t handle, const char *key, uint16_t *value) {

    esp_err_t err = nvs_get_u16(handle, key, value);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded %s: %u.", key, *value);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No entry for %s, using defaults: %u", key, *value);
    } else {
        ESP_LOGE(TAG, "Error loading %s: %s", key, esp_err_to_name(err));
    }
}

static void save_setting(const char *key, uint16_t value) {

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


static void setup_nvs(void) {

    ESP_LOGI(TAG, "Starting NVS and loading settings");

    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    ESP_ERROR_CHECK(err);

    nvs_handle_t nvs;

    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

    load_setting(nvs, NVS_KEY_DUTY_MIN, &led_duty_min);
    load_setting(nvs, NVS_KEY_DUTY_MAX, &led_duty_max);
    load_setting(nvs, NVS_KEY_FADE_TIME_MS, &led_fade_time_ms);

    nvs_close(nvs);

}


static void setup_led(void) {

    ESP_LOGI(TAG, "Setting up LED");

    ledc_timer_config_t led_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .freq_hz = LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };

    ESP_ERROR_CHECK(ledc_timer_config(&led_timer));

    ledc_channel_config_t led_channel = {
        .gpio_num = LED_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = led_timer.timer_num,
        .duty = 0,
    };

    ESP_ERROR_CHECK(ledc_channel_config(&led_channel));
    ESP_ERROR_CHECK(ledc_fade_func_install(0));

}


static void setup_gpio(void) {

    ESP_LOGI(TAG, "Setting up GPIO PINS");

    gpio_config_t button_cfg = {
        .pin_bit_mask   = 1ULL << BUTTON_GPIO,
        .mode           = GPIO_MODE_INPUT,
        .pull_up_en     = GPIO_PULLUP_ENABLE,
        .pull_down_en   = GPIO_PULLDOWN_DISABLE,
        .intr_type      = GPIO_INTR_DISABLE,
    };

    gpio_config_t vibe_cfg = {
        .pin_bit_mask   = 1ULL << VIBE_GPIO,
        .mode           = GPIO_MODE_OUTPUT,
        .pull_up_en     = GPIO_PULLUP_DISABLE,
        .pull_down_en   = GPIO_PULLDOWN_DISABLE,
        .intr_type      = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&button_cfg));
    ESP_ERROR_CHECK(gpio_config(&vibe_cfg));

    ESP_LOGI(TAG, "GPIO Setup Complete.  Button: GPIO_%d  Vibe: GPIO_%d", BUTTON_GPIO, VIBE_GPIO);

    gpio_set_level(VIBE_GPIO, 0);

}


static void vibeTask(void *arg) {
    
    int pulses;

    while(1) {
        if(xQueueReceive(vibeQueue, &pulses, portMAX_DELAY)) {
            for(int i = 0; i < pulses; i++) {
                gpio_set_level(VIBE_GPIO, 1);
                vTaskDelay(pdMS_TO_TICKS(VIBE_PULSE_ON));
                gpio_set_level(VIBE_GPIO, 0);
                vTaskDelay(pdMS_TO_TICKS(VIBE_PULSE_OFF));
            }
        }
    }

}


static void fadeTask(void *arg) {

    while(1) {
        ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, led_duty_max, led_fade_time_ms);
        ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_WAIT_DONE);
        ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, led_duty_min, led_fade_time_ms);
        ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_WAIT_DONE);
    }
}


void app_main(void)
{
    setup_gpio();
    setup_led();
    setup_nvs();

    vibeQueue = xQueueCreate(4, sizeof(int));
    xTaskCreate(vibeTask, "vibe", 3072, NULL, 5, NULL);
    xTaskCreate(fadeTask, "fade", 3072, NULL, 3, NULL);

    bool isLongPress = false;
    bool isLongerPress = false;
    int last_reading = 1;
    int64_t press_time = 0;

        
    while (1) {

        int button_value = gpio_get_level(BUTTON_GPIO);
        

        if (button_value != last_reading) {
    
            if (button_value == 0) {
                press_time = esp_timer_get_time();
                isLongPress = false;
                isLongerPress = false;
                
            } else {
                int64_t held_time = (esp_timer_get_time() - press_time) / 1000;

                if (held_time >= LONGER_PRESS_MS) {
                    // LONGER PRESS
                    ESP_LOGI(TAG, "Longer Press Detected");
                
                } else if (held_time >= LONG_PRESS_MS) {
                    // LONG PRESS
                    ESP_LOGI(TAG, "Long Press Detected");
                
                } else {
                    // SHORT PRESS
                    ESP_LOGI(TAG, "Short Press Detected");
                }

                ESP_LOGI(TAG, "Button held for %lld ms.", (long long)held_time);
            }
    
            ESP_LOGI(TAG, "Button State: %d", button_value);
            last_reading = button_value;
        }

        if (button_value == 0) {

            int64_t elapsed_ms = (esp_timer_get_time() - press_time) / 1000;

            if (elapsed_ms >= LONG_PRESS_MS) {
                if (!isLongPress) {
                    isLongPress = true;
                    ESP_LOGI(TAG, "Long Press Buzz 1");
                    int pulses = 1;
                    xQueueSend(vibeQueue, &pulses, 0);
                }
            } 
            
            if (elapsed_ms >= LONGER_PRESS_MS) {
                if (!isLongerPress) {
                    isLongerPress = true;
                    ESP_LOGI(TAG, "Longer Press Buzz 2");
                    int pulses = 2;
                    xQueueSend(vibeQueue, &pulses, 0);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));

    }

}