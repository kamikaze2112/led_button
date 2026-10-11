# led_button

ESP32-C6 firmware for a bedside button with a pulsing LED and a vibration motor. Button presses show up in Home Assistant as an event entity, the LED behavior is adjustable from Home Assistant, and the firmware can be updated over Wi-Fi.

Written in C with ESP-IDF 6.1. No ESPHome, no Arduino.

## Features

- Appears in Home Assistant as one device through MQTT Discovery. No YAML on the Home Assistant side.
- Three press types reported as events: short, long, and longer.
- Vibration feedback while holding the button, so you know which press you're about to get.
- LED fades between a lower and an upper brightness. Both levels and the fade time are adjustable from Home Assistant and survive reboots.
- Availability tracking through an MQTT last will, so Home Assistant shows the device as unavailable when it drops off.
- Over-the-air updates from a small web page on the device, started by a button in Home Assistant.
- Firmware version and running OTA slot shown in the device info panel.

## Hardware

| Part | Pin | Notes |
| --- | --- | --- |
| Button | GPIO1 | Active low, internal pull-up enabled. Wire between GPIO1 and ground. |
| LED | GPIO2 | LEDC PWM, 4 kHz, 13-bit. |
| Vibration motor | GPIO3 | Output, high is on. Drive the motor through a transistor or driver, not straight from the pin. |

The board is an ESP32-C6 with 8 MB of flash.

## Press behavior

| Press | Held for | Feedback |
| --- | --- | --- |
| Short | under 1 s | none |
| Long | 1 s or more | 1 buzz when the threshold is reached |
| Longer | 2.5 s or more | a second buzz when the threshold is reached |

The event is published when the button is released.

## Home Assistant

Requires the MQTT integration with a broker, and discovery left on its default `homeassistant` prefix.

The device is named `button_` followed by the last three bytes of its MAC address, for example `button_a1b2c3`. It creates these entities:

| Entity | Type | Range | Default |
| --- | --- | --- | --- |
| Press | event (`short_press`, `long_press`, `longer_press`) | | |
| LED PWM Lower | number (slider) | 0 to 100 % | 1 % |
| LED PWM Upper | number (slider) | 0 to 100 % | 50 % |
| Fade Time (ms) | number (slider) | 0 to 5000 ms | 2500 ms |
| OTA | button | | |

To make the LED hold steady instead of pulsing, set Lower and Upper to the same value.

Use the Press event as a trigger in automations: choose "Event received" and pick the event type.

### Topics

All topics start with the device name (`<uid>` below).

| Topic | Direction | Purpose |
| --- | --- | --- |
| `<uid>/status` | device to broker | `online` / `offline`, retained. Used as the availability topic and last will. |
| `<uid>/button` | device to broker | Press events as JSON, `{"event_type":"short_press"}`. Not retained, QoS 0. |
| `<uid>/led_min/set`, `<uid>/led_max/set`, `<uid>/fade/set` | broker to device | Slider commands. |
| `<uid>/led_min/state`, `<uid>/led_max/state`, `<uid>/fade/state` | device to broker | Current values, retained. |
| `<uid>_ota` | broker to device | Starts OTA mode. |
| `homeassistant/device/<uid>/config` | device to broker | Discovery message, retained. |
| `homeassistant/status` | broker to device | When Home Assistant announces `online`, the device re-sends discovery and state. |

## Building

Requires ESP-IDF 6.1 and an ESP32-C6.

1. Copy `main/secrets.h.example` to `main/secrets.h` and fill in your Wi-Fi and broker details. `secrets.h` is gitignored.
2. Set the target and build:

   ```
   idf.py set-target esp32c6
   idf.py build
   ```

3. Flash and watch the log:

   ```
   idf.py -p <port> flash monitor
   ```

The project uses the custom partition table in `partitions.csv`: two 2 MB application slots (`ota_0` and `ota_1`), plus NVS, OTA data, and PHY. That needs the flash size set to 8 MB and the partition table set to Custom in menuconfig (Serial Flasher Config and Partition Table). If you change the partition table on a board that already has firmware, run `idf.py -p <port> erase-flash` before flashing.

## Updating over the air

1. Bump `PROJECT_VER` in the top-level `CMakeLists.txt` and run `idf.py build`.
2. In Home Assistant, press the OTA button on the device. The LED starts blinking, which means the update server is up.
3. Open the device page in Home Assistant and click Visit, or browse to the device's IP address.
4. Choose `build/led_button.bin` and click Upload.
5. The device reboots into the new firmware. The device info panel then shows the new version and the other slot, for example `Firmware: 1.1.0 (ota_1)`.

If nobody uploads anything, the server shuts itself off after 3 minutes and the LED goes back to normal.

You can also upload from a terminal:

```
curl -H "Expect:" --data-binary @build/led_button.bin http://<device ip>/update
```

The update page has no password. Only use it on a network you trust. Firmware images contain the Wi-Fi and broker credentials from `secrets.h`, so don't publish built `.bin` files.

## Project layout

| File | What it does |
| --- | --- |
| `main/main.c` | Startup order |
| `main/wifi.c` | Wi-Fi station connection and reconnects |
| `main/mqtt.c` | MQTT client, discovery, commands, state, availability |
| `main/button.c` | Button reading and press timing |
| `main/vibe.c` | Vibration motor pulses |
| `main/led.c` | PWM fading, blink while in OTA mode, saved settings |
| `main/settings.c` | Saving and loading values in NVS |
| `main/ota.c` | OTA web server, upload handler, timeout |
