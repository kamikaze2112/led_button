#include "vibe.h"
#include "settings.h"
#include "wifi.h"
#include "led.h"
#include "button.h"


void app_main(void)
{

    // These need to be called in this order to prevent things from going sideways.
    
    settings_setup();
    wifi_setup();
    led_setup();
    vibe_setup();
    button_setup();

}