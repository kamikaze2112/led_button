#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "led.h"
#include "settings.h"

#define NVS_KEY_DUTY_MIN            "led_min"
#define NVS_KEY_DUTY_MAX            "led_max"
#define NVS_KEY_FADE_TIME_MS        "fade_time"

#define LED_GPIO                    GPIO_NUM_2

#define LEDC_FREQ_HZ                4000

#define LED_DUTY_MIN_DEFAULT        10
#define LED_DUTY_MAX_DEFAULT        4096
#define LED_FADE_TIME_MS_DEFAULT    2500

#define LED_SPEED_MODE              LEDC_LOW_SPEED_MODE
#define LED_CHANNEL                 LEDC_CHANNEL_0

static const char *TAG = "led";

static uint16_t led_duty_min = LED_DUTY_MIN_DEFAULT;
static uint16_t led_duty_max = LED_DUTY_MAX_DEFAULT;
static uint16_t led_fade_time_ms = LED_FADE_TIME_MS_DEFAULT;

static void fade_task(void *arg) {

    while(1) {
        ledc_set_fade_with_time(LED_SPEED_MODE, LED_CHANNEL, led_duty_max, led_fade_time_ms);
        ledc_fade_start(LED_SPEED_MODE, LED_CHANNEL, LEDC_FADE_WAIT_DONE);
        ledc_set_fade_with_time(LED_SPEED_MODE, LED_CHANNEL, led_duty_min, led_fade_time_ms);
        ledc_fade_start(LED_SPEED_MODE, LED_CHANNEL, LEDC_FADE_WAIT_DONE);
    }
}

void led_setup(void) {

    ESP_LOGI(TAG, "Setting up LED");

    ESP_LOGI(TAG, "Grabbing values from NVS if available...");

    settings_load(NVS_KEY_DUTY_MIN, &led_duty_min);
    settings_load(NVS_KEY_DUTY_MAX, &led_duty_max);
    settings_load(NVS_KEY_FADE_TIME_MS, &led_fade_time_ms);
    
    ledc_timer_config_t led_timer = {
        .speed_mode = LED_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .freq_hz = LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };

    ESP_ERROR_CHECK(ledc_timer_config(&led_timer));

    ledc_channel_config_t led_channel = {
        .gpio_num = LED_GPIO,
        .speed_mode = LED_SPEED_MODE,
        .channel = LED_CHANNEL,
        .timer_sel = led_timer.timer_num,
        .duty = 0,
    };

    ESP_ERROR_CHECK(ledc_channel_config(&led_channel));
    ESP_ERROR_CHECK(ledc_fade_func_install(0));

    xTaskCreate(fade_task, "fade", 3072, NULL, 3, NULL);

    ESP_LOGI(TAG, "LED setup complete.");

}

