<div align="center">

# ESPHome Panasonic AC

### Local control for Panasonic air conditioners via ESPHome

[![ESPHome][esphome-badge]][esphome-url]
[![Home Assistant][ha-badge]][ha-url]

</div>

---

An open source alternative for the Panasonic wi-fi adapter that works locally without the cloud. Provides a drop-in replacement for the Panasonic DNSK-P11 and CZ-TACG1 wifi modules using an ESP32 via the UART interface.

---

## 🛠️ What's different from the original

This is a maintained fork of [DomiStyle/esphome-panasonic-ac](https://github.com/DomiStyle/esphome-panasonic-ac) with ESPHome 2026.x compatibility, bug fixes and new features.

### Compatibility
- ⚡ **ESPHome 2026.4+ support** — custom fan modes and presets are registered via `setup()` (new API). The deprecated `traits()` registration has been removed after confirming `setup()`-only works in ESPHome 2026.5.1. No more deprecation warnings during build.

### New features
- 🌬️ **Auto Comfort preset** (CNT) — adds the Auto Comfort preset mode alongside Normal, Powerful and Quiet. Reverse-engineered from UART logs
- 🌡️ **Inside temperature sensor** — exposes the AC's internal temperature reading as a standalone entity for automations, dashboards and history graphs
- ❄️ **Defrost sensor** — binary sensor that reports when the AC unit is in defrost mode (CNT + WLAN)
- 🔧 **Temperature offset for WLAN** — `current_temperature_offset` now works for both CNT and WLAN (was CNT-only)
- 🎚️ **Live Temperature Offsets** — dynamically adjust `current_temperature_offset` and `outside_temperature_offset` from Home Assistant via new number entities without recompiling.
- 🛡️ **Temperature Sensor Fallback** — if the external `current_temperature_sensor` becomes unavailable (NaN), the integration automatically falls back to the AC's internal temperature sensor.

### Bug fixes
- 🐛 **CNT climate action always "Off"** — `determine_action()` was never called in the CNT poll handler, so Home Assistant always showed "Off (Cooling)" instead of the actual action (Cooling, Heating, Idle)
- 🐛 **Preset reset mask missing Auto Comfort** (CNT) — changing fan mode used mask `0xF0` which didn't clear the Auto Comfort bit; fixed to `0xD0`
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

## 🔄 Migrating from DomiStyle/esphome-panasonic-ac

1. **Change `external_components`** in your YAML:
   ```yaml
   external_components:
     - source:
         type: git
         url: https://github.com/duczz/ha-esphome-panasonic-ac
       components: [panasonic_ac]
   ```

2. **Clear ESPHome build cache**: Click **Clean Build Files** in the ESPHome dashboard (three-dot menu on your device). Without this, ESPHome uses the cached old component.

3. **Update YAML lambdas** if you access `custom_fan_mode` or `custom_preset` in `on_state` or other lambdas:
   ```cpp
   // Old (crashes in ESPHome 2026.x):
   if (id(ac).custom_fan_mode.has_value()) {
     std::string fan_mode = id(ac).custom_fan_mode.value();

   // New:
   auto fan_mode = id(ac).get_custom_fan_mode();
   if (!fan_mode.empty()) {
     // fan_mode is a StringRef, compare directly with "..."
   ```
   **Do NOT compare with `nullptr`** — `StringRef` comparison with `nullptr` causes a crash.

4. **Compile and flash**.

---

## Table of Contents

- [What's different](#️-whats-different-from-the-original)
- [Migrating from DomiStyle](#-migrating-from-domistyleesphome-panasonic-ac)
- [Features](#-features)
- [Supported hardware](#-supported-hardware)
- [Requirements](#-requirements)
- [Software installation](#-software-installation)
- [Configuration](#️-configuration)
  - [All options](#all-options)
  - [Example configuration](#example-configuration)
  - [Temperature offsets](#temperature-offsets)
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
* Fallback to internal sensor if external temperature sensor fails
* Live temperature offset adjustment via Home Assistant sliders

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

### Clearing the ESPHome cache

ESPHome caches external components aggressively. After switching to this fork (or after updates), clear the cache before compiling: click **Clean Build Files** in the ESPHome dashboard (three-dot menu on your device).

If your build completes in under 15 seconds, the cache was NOT cleared.

---

## ⚙️ Configuration

### All options

| Option | Type | Availability | Description |
|--------|------|:------------:|-------------|
| `type` | string | **required** | `cnt` (CZ-TACG1) or `wlan` (DNSK-P11) |
| `name` | string | **required** | Entity name in Home Assistant |
| `horizontal_swing_select` | Select | CNT + WLAN | Manual horizontal swing position (auto, left, left_center, center, right_center, right) |
| `vertical_swing_select` | Select | CNT + WLAN | Manual vertical swing position (swing, auto, up, up_center, center, down_center, down) |
| `outside_temperature` | Sensor | CNT + WLAN | Outside temperature as reported by the AC |
| `inside_temperature` | Sensor | CNT + WLAN | Inside temperature from the AC's internal sensor, as standalone entity |
| `nanoex_switch` | Switch | CNT + WLAN | Toggle Panasonic nanoeX air purification. **nanoe-G is not supported** — the CN-CNT protocol does not expose it (see [nanoe-G note](#nanoe-g)) |
| `eco_switch` | Switch | CNT only | Toggle energy-saving mode (simple power reduction) |
| `econavi_switch` | Switch | CNT only | Toggle Econavi (sensor-based activity detection that auto-adjusts power) |
| `mild_dry_switch` | Switch | CNT only | Toggle mild dry cooling (reduced dehumidification) |
| `current_power_consumption` | Sensor | CNT only | Estimated power consumption in watts |
| `defrost_sensor` | Binary Sensor | CNT + WLAN | Reports `on` when the AC is in defrost mode |
| `current_temperature_offset` | int | CNT + WLAN | Fixed offset for inside temperature (-15 to +15) |
| `outside_temperature_offset` | int | CNT + WLAN | Fixed offset for outside temperature (-15 to +15) |
| `current_temperature_offset_number` | Number | CNT + WLAN | Exposes a slider to Home Assistant to dynamically change the inside temperature offset |
| `outside_temperature_offset_number` | Number | CNT + WLAN | Exposes a slider to Home Assistant to dynamically change the outside temperature offset |
| `current_temperature_sensor` | Sensor ID | CNT only | Use an external sensor for current temperature instead of the AC's internal sensor |

> **Eco vs. Econavi:** Eco is a simple power reduction mode. Econavi is Panasonic's smart sensor feature that detects room activity (human presence, sunlight) and auto-adjusts power accordingly. They are independent features.

> <a name="nanoe-g"></a>**nanoe-G is not supported on CZ-TACG1 (CNT):** The CN-CNT serial protocol does not transmit the nanoe-G state — pressing the nanoe-G button on the IR remote produces no change in the UART data stream. It may be accessible via the DNSK-P11 WLAN protocol (CN-WLAN) since the Comfort Cloud API includes a dedicated NanoeMode field, but this is unconfirmed and not implemented.

> **Enabling unsupported features can lead to undefined behavior. Check your remote or manual first.**

> **`current_power_consumption` is an estimated value by the AC, not a measured value.**

### Example configuration

```yaml
climate:
  - platform: panasonic_ac
    type: cnt
    name: "Living Room AC"
    horizontal_swing_select:
      name: "Horizontal Swing"
    vertical_swing_select:
      name: "Vertical Swing"
    outside_temperature:
      name: "Outside Temperature"
    inside_temperature:
      name: "Inside Temperature"
    nanoex_switch:
      name: "NanoeX"
    econavi_switch:
      name: "Econavi"
    current_power_consumption:
      name: "Power Consumption"
    defrost_sensor:
      name: "Defrost"
    current_temperature_offset: 0
    outside_temperature_offset: 0
    current_temperature_offset_number:
      name: "Inside Temperature Offset"
    outside_temperature_offset_number:
      name: "Outside Temperature Offset"
```

### Temperature offsets

The AC's internal sensors may not reflect the actual room temperature. You can define a fixed offset for both inside and outside temperature. The offset is applied to both reported values and target temperature commands:
- Reported temperature = AC reading + offset
- Target sent to AC = user target - offset

**You have two options for configuring offsets:**
1. **Static (Fixed) Offset:** Use `current_temperature_offset: 2`. This bakes the offset into the firmware.
2. **Dynamic (Slider) Offset:** Use `current_temperature_offset_number`. This creates a slider in Home Assistant to change the offset live, without recompiling.

*Note: You do not need to use both. If you only want the slider, you can delete the static `current_temperature_offset` from your YAML. If you use both, the static value acts as the default boot value for the slider!*

| Situation | Example | Offset |
|-----------|---------|--------|
| Room is warmer than AC reports | Actual 23°, AC reads 20° | `+3` |
| Room is cooler than AC reports | Actual 20°, AC reads 22° | `-2` |

---

## 🔧 Hardware installation

[Hardware installation for DNSK-P11](README.DNSKP11.md)

[Hardware installation for CZ-TACG1](README.CZTACG1.md)

---

[esphome-badge]: https://img.shields.io/badge/ESPHome-2026.4%2B-000000?style=for-the-badge&logo=esphome
[esphome-url]: https://esphome.io/
[ha-badge]: https://img.shields.io/badge/Home%20Assistant-2026.4%2B-blue?style=for-the-badge&logo=home-assistant
[ha-url]: https://www.home-assistant.io/
