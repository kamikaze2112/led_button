#pragma once

#include <stdint.h>

void settings_setup();
void settings_load(const char *key, uint16_t *value);
void settings_save(const char *key, uint16_t value);