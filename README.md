<div align="center">

# ESPHome Panasonic AC

### Local control for Panasonic air conditioners via ESPHome

[![ESPHome][esphome-badge]][esphome-url]
[![Home Assistant][ha-badge]][ha-url]

</div>

---

An open source alternative for the Panasonic wi-fi adapter that works locally without the cloud. Provides a drop-in replacement for the Panasonic DNSK-P11 and CZ-TACG1 wifi modules using an ESP32 via the UART interface.

---

## Table of Contents

- [About this fork](#️-about-this-fork)
- [Migrating from DomiStyle](#-migrating-from-domistyleesphome-panasonic-ac)
- [Features](#-features)
- [Supported hardware](#-supported-hardware)
- [Requirements](#-requirements)
- [Software installation](#-software-installation)
- [Configuration](#️-configuration)
  - [All options](#all-options)
  - [Entities and supported values](#entities-and-supported-values)
  - [Example configuration](#example-configuration)
  - [Daily energy consumption (kWh)](#daily-energy-consumption-kwh)
  - [Temperature offsets](#temperature-offsets)
- [Hardware installation](#-hardware-installation)

---

## 🛠️ About this fork

This is a maintained, modernised fork of [DomiStyle/esphome-panasonic-ac](https://github.com/DomiStyle/esphome-panasonic-ac), rebuilt for the latest ESPHome (2026.x) and Home Assistant versions. It adds a range of new features, bug fixes and stability improvements — from the Auto Comfort preset and inside temperature sensor to compressor-based climate action and live temperature offsets.

For a detailed list of all new features, bug fixes, and improvements, please check the [CHANGELOG.md](CHANGELOG.md).

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
| `current_temperature_sensor` | Sensor ID | CNT only | Use an external sensor for current temperature instead of the AC's internal sensor (see [details](#using-an-external-temperature-sensor)) |
| `compressor_action` | bool | CNT only | Derive the climate action (Cooling/Heating/Idle) from the AC's real compressor state instead of the temperature heuristic. Default `false`. Falls back automatically if the AC model reports an unexpected value. During a defrost cycle the action is reported as `Idle`, since the unit is not conditioning the room |

> **Eco vs. Econavi:** Eco is a simple power reduction mode. Econavi is Panasonic's smart sensor feature that detects room activity (human presence, sunlight) and auto-adjusts power accordingly. They are independent features.

> <a name="nanoe-g"></a>**nanoe-G is not supported on CZ-TACG1 (CNT):** The CN-CNT serial protocol does not transmit the nanoe-G state — pressing the nanoe-G button on the IR remote produces no change in the UART data stream. It may be accessible via the DNSK-P11 WLAN protocol (CN-WLAN) since the Comfort Cloud API includes a dedicated NanoeMode field, but this is unconfirmed and not implemented.

> **Enabling unsupported features can lead to undefined behavior. Check your remote or manual first.**

> **`current_power_consumption` is an estimated value by the AC, not a measured value.** It covers the whole split system (indoor + outdoor unit) — it stays non-zero when only the indoor fan runs.

> **Multi-split systems:** If several indoor units share a single outdoor unit, do not blindly add up the sensors (or any kWh values derived from them) of all indoor units — each adapter may report the whole system's consumption, which would count the same energy several times. This is unverified; no multi-split system was available for testing. Reports welcome in an issue. With separate single-split systems (one outdoor unit each), summing is correct.

### Entities and supported values

These are the exact values the component registers. Use these strings verbatim in automations, scripts and template selects — Home Assistant matches them literally.

| | Supported values |
|---|---|
| **Modes** | `off`, `heat_cool`, `cool`, `heat`, `fan_only`, `dry` |
| **Fan modes** (custom) | `Automatic`, `1`, `2`, `3`, `4`, `5` |
| **Presets** (custom) | `Normal`, `Powerful`, `Quiet`, `Auto Comfort` |
| **Swing modes** | `off`, `both`, `vertical`, `horizontal` |
| **Vertical swing select** | `swing`, `auto`, `up`, `up_center`, `center`, `down_center`, `down` |
| **Horizontal swing select** | `auto`, `left`, `left_center`, `center`, `right_center`, `right` |
| **Target temperature** | 16 – 30 °C in steps of 0.5 °C |

Fan modes and presets are *custom* values, so in a lambda they come back as a `StringRef`:

```cpp
auto fan_mode = id(my_ac).get_custom_fan_mode();
if (!fan_mode.empty() && fan_mode == "Automatic") { /* ... */ }
```

The two swing selects are separate entities from the climate `swing_mode`: the selects set a fixed louver position, while `swing_mode` reports `vertical` / `horizontal` / `both` whenever the corresponding select is on `auto`.

> **Update rate:** CZ-TACG1 (`type: cnt`) polls the AC every **5 seconds**, DNSK-P11 (`type: wlan`) every **30 seconds**. State changes made on the IR remote therefore show up noticeably faster on CNT. Commands you send are transmitted immediately on both.

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
    compressor_action: true
    current_temperature_offset: 0
    outside_temperature_offset: 0
    current_temperature_offset_number:
      name: "Inside Temperature Offset"
    outside_temperature_offset_number:
      name: "Outside Temperature Offset"
```

### Daily energy consumption (kWh)

The component reports **instantaneous power in watts only**. To get a kWh value for Home Assistant's Energy dashboard, stack ESPHome's standard [`total_daily_energy`](https://esphome.io/components/sensor/total_daily_energy.html) platform on top of the power sensor. Two things are required: give `current_power_consumption` an `id`, and have a `time:` source in your config (the counter resets at local midnight).

```yaml
climate:
  - platform: panasonic_ac
    type: cnt
    # ... rest of your config
    current_power_consumption:
      id: ac_power          # required so total_daily_energy can reference it
      name: "Power Consumption"

sensor:
  - platform: total_daily_energy
    name: "Total Daily Energy"
    power_id: ac_power
    unit_of_measurement: kWh
    device_class: energy
    filters:
      - multiply: 0.001     # W -> kW, so the daily total comes out in kWh

time:
  - platform: homeassistant
    id: homeassistant_time
```

You do not need to set `unit_of_measurement`, `accuracy_decimals`, `device_class` or `state_class` on `current_power_consumption` — the component already declares them (`W`, `0`, `power`, `measurement`).

Since the kWh value is derived from the estimated power reading, it inherits both caveats above: it is an estimate, not a meter reading, and on multi-split systems it must not be summed across indoor units.

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

#### Using an external temperature sensor

With `current_temperature_sensor` (CNT only) the climate entity uses any other Home Assistant / ESPHome sensor as its current temperature instead of the AC's internal one. The component falls back to the AC's internal reading whenever the external sensor is **not usable**, which means any of:

* no external sensor is configured,
* the sensor has not published a value yet (this is the normal state right after boot),
* its value is `NaN` (typical for an unavailable sensor).

> **The offset is applied to the external sensor too.** `current_temperature_offset` is added on top of whatever value the external sensor reports, not only to the AC's internal reading. If you already use a calibrated external sensor, leave the offset at `0` — otherwise you shift an already correct value a second time.

#### Implausible readings are discarded

A temperature sensor that never updates is usually being filtered, not stuck. Two independent filters can cause this:

* **Unsupported by the unit** — the AC sends the placeholder `0x80` for a sensor it does not have. The value is skipped and `... temperature is not supported` is logged at `VERBOSE`. Not all models report an outside temperature.
* **Implausible value** — anything above 100 °C after the offset is discarded with a `Received out of range ...` warning, so the entity keeps its last good value instead of publishing nonsense.

If a temperature looks frozen, set `logger: level: VERBOSE` and check which of the two messages appears.

---

## 🔧 Hardware installation

[Hardware installation for DNSK-P11](README.DNSKP11.md)

[Hardware installation for CZ-TACG1](README.CZTACG1.md)

---

[esphome-badge]: https://img.shields.io/badge/ESPHome-2026.4%2B-000000?style=for-the-badge&logo=esphome
[esphome-url]: https://esphome.io/
[ha-badge]: https://img.shields.io/badge/Home%20Assistant-2026.4%2B-blue?style=for-the-badge&logo=home-assistant
[ha-url]: https://www.home-assistant.io/
