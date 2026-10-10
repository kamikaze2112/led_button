#pragma once

typedef enum {
    MQTT_SHORT_PRESS,
    MQTT_LONG_PRESS,
    MQTT_LONGER_PRESS,
} press_t;



void mqtt_setup();
void mqtt_button(press_t press_t);
