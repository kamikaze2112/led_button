#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "led.h"
#include "settings.h"
#include <stdint.h>

#define NVS_KEY_PCT_MIN             "pwm_min"
#define NVS_KEY_PCT_MAX             "pwm_max"
#define NVS_KEY_FADE_TIME_MS        "fade_time"

#define LED_GPIO                    GPIO_NUM_2

#define LEDC_FREQ_HZ                4000

#define LED_MIN_PCT_DEFAULT         1
#define LED_MAX_PCT_DEFAULT         50
#define LED_FADE_TIME_MS_DEFAULT    2500

#define LED_SPEED_MODE              LEDC_LOW_SPEED_MODE
#define LED_CHANNEL                 LEDC_CHANNEL_0

static const char *TAG = "led";

static uint16_t led_min_pct = LED_MIN_PCT_DEFAULT;
static uint16_t led_max_pct = LED_MAX_PCT_DEFAULT;
static uint16_t led_fade_time_ms = LED_FADE_TIME_MS_DEFAULT;

static uint32_t pct_to_duty(uint16_t pct) {
    return pct * 8191 / 100;
}

static void fade_task(void *arg) {

    while(1) {
        ledc_set_fade_with_time(LED_SPEED_MODE, LED_CHANNEL, pct_to_duty(led_max_pct), led_fade_time_ms);
        ledc_fade_start(LED_SPEED_MODE, LED_CHANNEL, LEDC_FADE_WAIT_DONE);
        ledc_set_fade_with_time(LED_SPEED_MODE, LED_CHANNEL, pct_to_duty(led_min_pct), led_fade_time_ms);
        ledc_fade_start(LED_SPEED_MODE, LED_CHANNEL, LEDC_FADE_WAIT_DONE);
    }
}

void led_setup(void) {

    ESP_LOGI(TAG, "Setting up LED");

    ESP_LOGI(TAG, "Grabbing values from NVS if available...");

    settings_load(NVS_KEY_PCT_MIN, &led_min_pct);
    settings_load(NVS_KEY_PCT_MAX, &led_max_pct);
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

void led_set_min(uint16_t pct) {
    if (pct > 100) pct = 100;
    led_min_pct = pct;
    settings_save(NVS_KEY_PCT_MIN, led_min_pct);
}

void led_set_max(uint16_t pct) {
    if (pct > 100) pct = 100;
    led_max_pct = pct;
    settings_save(NVS_KEY_PCT_MAX, led_max_pct);
}

void led_set_fade(uint16_t ms) {
    led_fade_time_ms = ms;
    settings_save(NVS_KEY_FADE_TIME_MS, led_fade_time_ms);
}

uint16_t led_get_min(void) { return led_min_pct; }
uint16_t led_get_max(void) { return led_max_pct; }
uint16_t led_get_fade(void) { return led_fade_time_ms; }