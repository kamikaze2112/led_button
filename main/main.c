#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/queue.h"
#include "driver/ledc.h"
#include "vibe.h"
#include "settings.h"
#include "wifi.h"
#include "led.h"
#include "button.h"


void app_main(void)
{

    settings_setup();
    wifi_setup();
    led_setup();
    vibe_setup();
    button_setup();

}