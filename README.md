<div align="center">

# ESPHome Panasonic AC

### Local control for Panasonic air conditioners via ESPHome

[![ESPHome][esphome-badge]][esphome-url]
[![Home Assistant][ha-badge]][ha-url]
[![License][license-badge]](LICENSE)

</div>

---

An open source alternative for the Panasonic wi-fi adapter that works locally without the cloud. Provides a drop-in replacement for the Panasonic DNSK-P11 and CZ-TACG1 wifi modules using an ESP32 via the UART interface.

---

## 🛠️ What's different from the original

This is a maintained fork of [DomiStyle/esphome-panasonic-ac](https://github.com/DomiStyle/esphome-panasonic-ac) with ESPHome 2026.x compatibility, bug fixes and new features.

### Compatibility
- ⚡ **ESPHome 2026.4+ support** — moved deprecated `set_supported_custom_fan_modes()` / `set_supported_custom_presets()` from `ClimateTraits` to the Climate entity (old code will fail to compile starting with ESPHome 2026.11.0)

### New features
- 🌡️ **Inside temperature sensor** — exposes the AC's internal temperature reading as a standalone entity for automations, dashboards and history graphs
- ❄️ **Defrost sensor** — binary sensor that reports when the AC unit is in defrost mode (CNT + WLAN)
- 🔧 **Temperature offset for WLAN** — `current_temperature_offset` now works for both CNT and WLAN (was CNT-only)

### Bug fixes
- 🐛 **Econavi toggle destroying preset + nanoex state** (CNT) — `on_econavi_change` overwrote the entire shared byte instead of toggling only its bit
- 🐛 **Nanoex toggle destroying econavi state** (CNT) — `on_nanoex_change` cleared the econavi bit when toggling
- 🐛 **`determine_action()` broken in HEAT_COOL mode** — never returned IDLE and showed COOLING even below target temperature. Reworked with proper deadband logic
- 🛡️ **WLAN report parser out-of-bounds** — added bounds check in key-value pair loop to prevent crash from malformed packets
- 🛡️ **CNT poll response out-of-bounds** — `set_data()` accessed `rx_buffer_` indices up to 30 without verifying packet size
- 🛡️ **Unbounded receive buffer growth** — `rx_buffer_` had no size limit, UART noise could cause OOM on the ESP32
- 🛡️ **Uninitialized WLAN command pointer** — `last_command_` risked a crash if `handle_resend()` fired before any command was sent
- 🔧 **Uninitialized swing state / timer members** — could cause first swing update to be skipped or unexpected timing behavior
- 🔧 **`log_packet` unnecessary vector copy** — changed to pass-by-reference

For the full version history see [CHANGELOG.md](CHANGELOG.md).

---

## Table of Contents

- [What's different](#️-whats-different-from-the-original)
- [Features](#-features)
- [Supported hardware](#-supported-hardware)
- [Requirements](#-requirements)
- [Software installation](#-software-installation)
- [Configuration](#️-configuration)
  - [Supported features](#setting-supported-features)
  - [Temperature offsets](#setting-temperature-offsets)
- [Hardware installation](#-hardware-installation)

---

## ✨ Features

* Control your AC locally via Home Assistant, MQTT or directly
* Instantly control the AC without any delay like in the Comfort Cloud app
* Receive live reports and state from the AC
* Uses the UART interface on the AC instead of the IR interface
* Provides a drop-in replacement for the Panasonic DNSK-P11 and the CZ-TACG1 wifi module
* Inside and outside temperature as standalone sensors
* Defrost status detection
* Power consumption monitoring (estimated by AC)

---

## 🔌 Supported hardware

This library works with both the **CN-CNT** port and the **CN-WLAN** port. CN-WLAN is only available on newer units. Either port can be used on units that have both ports regardless of the usage of the other port (ie. it is possible to leave the DNSK-P11 connected to CN-WLAN and connect the ESP to CN-CNT).

Works on the ESP8266 but **ESP32 is preferred** for the multiple hardware serial ports.

---

## 📦 Requirements

* ESP32 (or ESP8266) ([supported by ESPHome](https://esphome.io/#devices))
* 5V to 3.3V bi-directional Logic Converter (minimum 2 channels, available as pre-soldered prototyping boards)
* Female-Female Jumper cables
* Soldering iron
* Wires to solder from Logic converter to ESP
* Heat shrink
* **ESPHome 2026.4.0 or newer**
* **Home Assistant 2026.4.0 or newer**

---

## 🚀 Software installation

This software installation guide assumes some familiarity with ESPHome.

* Pull this repository or copy the `ac.yaml.example` from the root folder
* Rename the `ac.yaml.example` to `ac.yaml`
* Uncomment the `type` field depending on which AC protocol you want to use
* Adjust the `ac.yaml` to your needs
* Connect your ESP
* Run `esphome ac.yaml run` and choose your serial port (or do this via the Home Assistant UI)
* If you see the handshake messages being sent (DNSK-P11) or polling requests being sent (CZ-TACG1) in the log you are good to go
* Disconnect the ESP and continue with hardware installation

### Using as external component

Add this to your ESPHome YAML:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/duczz/ha-esphome-panasonic-ac
    components: [panasonic_ac]
```

---

## ⚙️ Configuration

### Setting supported features

Since Panasonic ACs support different features you can comment out the lines at the bottom of your `ac.yaml`:

```yaml
  # Enable as needed
  # eco_switch:
  #   name: Panasonic AC Eco Switch
  # nanoex_switch:
  #   name: Panasonic AC NanoeX Switch
  # mild_dry_switch:
  #   name: Panasonic AC Mild Dry Switch
  # econavi_switch:
  #   name: Panasonic AC Econavi Switch
  # current_power_consumption:
  #   name: Panasonic AC Power Consumption
  # inside_temperature:
  #   name: Panasonic AC Inside Temperature
  # defrost_sensor:
  #   name: Panasonic AC Defrost Status
```

In order to find out which features are supported by your AC, check the remote that came with it. Please note that eco switch and mild dry switch are not supported on DNSK-P11.

**Enabling unsupported features can lead to undefined behavior and may damage your AC. Make sure to check your remote or manual first.**

**current_power_consumption is just an ESTIMATED value by the AC**

### Setting temperature offsets

As the internal sensors reading might not reflect the actual temperature in the room or outside, you can optionally define a fixed offset for both sensors.
This offset is internally applied to both, the reported temperature in ESPHome/HomeAssistant as well as to the target temperature. Any shown values always include the defined offset.

- Temperature readings of the AC will be reported as (reading + offset)
- Target temperatures will be set as (target - offset)

```yaml
    # Adapt according to your measurements
    current_temperature_offset: 0
    outside_temperature_offset: 0
```

Examples:
- If the temperature is actually higher than measured by the AC, set the difference as a positive offset.
  - E.g. actual temperature = 23°, AC measured temperature = 20° --> offset = 3°
- If the temperature is actually lower than measured by the AC, set the difference as a negative offset.
  - E.g. actual temperature = 20°, AC measured temperature = 22° --> offset = -2°

---

## 🔧 Hardware installation

[Hardware installation for DNSK-P11](README.DNSKP11.md)

[Hardware installation for CZ-TACG1](README.CZTACG1.md)

---

[esphome-badge]: https://img.shields.io/badge/ESPHome-2026.4%2B-000000?style=for-the-badge&logo=esphome
[esphome-url]: https://esphome.io/
[ha-badge]: https://img.shields.io/badge/Home%20Assistant-2026.4%2B-blue?style=for-the-badge&logo=home-assistant
[ha-url]: https://www.home-assistant.io/
[license-badge]: https://img.shields.io/github/license/duczz/ha-esphome-panasonic-ac.svg?style=for-the-badge
