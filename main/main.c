#include "esp_event.h"
#include "vibe.h"
#include "settings.h"
#include "wifi.h"
#include "led.h"
#include "button.h"
#include "mqtt.h"


void app_main(void)
{

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // These need to be called in this order to prevent things from going sideways.
    
    settings_setup();
    mqtt_setup();
    wifi_setup();
    led_setup();
    vibe_setup();
    button_setup();

}