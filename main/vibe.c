
#include "vibe.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "freertos/queue.h"
#include "esp_log.h"

#define VIBE_GPIO                   GPIO_NUM_3
#define VIBE_PULSE_ON               200
#define VIBE_PULSE_OFF              100

static const char *TAG = "vibe";

static QueueHandle_t vibeQueue;

static void vibe_task(void *arg) {

    ESP_LOGI(TAG, "vibe_task");
    
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

void vibe_setup() {

        ESP_LOGI(TAG, "vibe_setup");

        gpio_config_t vibe_cfg = {
        .pin_bit_mask   = 1ULL << VIBE_GPIO,
        .mode           = GPIO_MODE_OUTPUT,
        .pull_up_en     = GPIO_PULLUP_DISABLE,
        .pull_down_en   = GPIO_PULLDOWN_DISABLE,
        .intr_type      = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&vibe_cfg));

    gpio_set_level(VIBE_GPIO, 0);

    vibeQueue = xQueueCreate(4, sizeof(int));
    xTaskCreate(vibe_task, "vibe", 3072, NULL, 5, NULL); 

    ESP_LOGI(TAG, "vibe_setup complete.");
}

void vibe_buzz(int pulses) {

    ESP_LOGI(TAG, "vibe_buzz");

    if (vibeQueue == NULL) {
        ESP_LOGE(TAG, "vibe_buzz called before vibe_setup");
        return;
    }
    
    xQueueSend(vibeQueue, &pulses, 0);
}
