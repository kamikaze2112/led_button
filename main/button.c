#include "button.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "vibe.h"
#include "mqtt.h"

#define LONG_PRESS_MS               1000
#define LONGER_PRESS_MS             2500

#define BUTTON_GPIO                 GPIO_NUM_1

static const char *TAG = "button";

static void button_task(void *arg) {


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
                    mqtt_button(MQTT_LONGER_PRESS);
                
                } else if (held_time >= LONG_PRESS_MS) {
                    // LONG PRESS
                    ESP_LOGI(TAG, "Long Press Detected");
                    mqtt_button(MQTT_LONG_PRESS);

                } else {
                    // SHORT PRESS
                    ESP_LOGI(TAG, "Short Press Detected");
                    mqtt_button(MQTT_SHORT_PRESS);
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
                    vibe_buzz(1);
                }
            } 
            
            if (elapsed_ms >= LONGER_PRESS_MS) {
                if (!isLongerPress) {
                    isLongerPress = true;
                    ESP_LOGI(TAG, "Longer Press Buzz 2");
                    vibe_buzz(2);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));

    }
}


void button_setup() {

    ESP_LOGI(TAG, "Button setup...");

    gpio_config_t button_cfg = {
        .pin_bit_mask   = 1ULL << BUTTON_GPIO,
        .mode           = GPIO_MODE_INPUT,
        .pull_up_en     = GPIO_PULLUP_ENABLE,
        .pull_down_en   = GPIO_PULLDOWN_DISABLE,
        .intr_type      = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&button_cfg));

    xTaskCreate(button_task, "button", 3072, NULL, 4, NULL); 

    ESP_LOGI(TAG, "button setup complete.");
}