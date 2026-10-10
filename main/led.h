#pragma once

void led_setup();
void led_set_min(uint16_t pct);
void led_set_max(uint16_t pct);
void led_set_fade(uint16_t ms);
uint16_t led_get_min(void);
uint16_t led_get_max(void); 
uint16_t led_get_fade(void);
