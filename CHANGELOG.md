# Changelog

## [2.6.2] – 2026-07-04

### ✨ New Features

- **Compressor-Based Climate Action (CNT, opt-in)** — The climate action (`Cooling` / `Heating` / `Idle`) now reflects the AC's **real compressor state** instead of a temperature guess. Home Assistant shows `Idle` when the room is satisfied and the compressor is off, and `Cooling` / `Heating` only when it is really running. Reverse-engineered from the CN-CNT poll response (byte 12: the high nibble encodes the mode, bit `0x04` the compressor state) and verified on a **Panasonic CS-E12QKEW** by correlating the byte against power draw across cool/heat/idle. **Off by default** — enable with `compressor_action: true` on a `type: cnt` climate. Other AC models are unaffected and fall back automatically if their reading is implausible; defrost cycles report `Idle`. Works at any logger level — the byte is read from the packet, not the log.

### 🐛 Bug Fixes

- **`%d` Format Specifier with a `float`** — An incorrect `%d` format specifier used with a `float` in a verbose debug log (`set_current_temperature_offset`) caused undefined behaviour that could print garbage. Logging only; no effect on AC control.

### 🔧 Code Quality

- **`°C` Unit on Offset Sliders** — The live temperature offset number entities (`current_temperature_offset_number` / `outside_temperature_offset_number`) now default to `°C` as their unit of measurement.
- **Internal Cleanup** — Added `#pragma once` include guards to the CNT/WLAN and command headers, gave `determine_power_consumption` internal linkage (`static`), removed no-op `(int8_t)` casts, and made `PanasonicACNumber::control` `protected` for consistency with the select/switch wrappers.

## [2.6.1] – 2026-05-31

### ✨ New Features

- **External Temperature Sensor Fallback** — If an external `current_temperature_sensor` becomes unavailable (e.g. reports `NaN` from an empty battery or a lost connection), the integration now gracefully falls back to the AC's internal temperature sensor until the external sensor recovers. Works on both CNT and WLAN.
- **Live Temperature Offset Adjustments** ([#179](https://github.com/DomiStyle/esphome-panasonic-ac/issues/179)) — Adjust `current_temperature_offset` and `outside_temperature_offset` dynamically from Home Assistant without recompiling ESPHome. The new `current_temperature_offset_number` and `outside_temperature_offset_number` options expose sliders directly in HA.
- **Auto Comfort Preset (CNT)** — The Auto Comfort preset can now be selected straight from Home Assistant. Selecting it sends bit `0x20` in the preset byte; switching back to Normal/Powerful/Quiet clears it cleanly via the `0xD0` mask. Reverse-engineered from UART logs by comparing the AC's response to the IR remote's Auto Comfort button.
- **Expanded `ac.yaml.example`** — Now ships with substitutions, the `esp-idf` framework, template selects for Fan Mode / Mode / Preset Mode with `on_state` sync lambdas, and a `refresh: always` hint for cache issues.

### 🐛 Bug Fixes

- **CNT Climate Action Always "Off"** — `determine_action()` was never called in the CNT poll handler, so the action stayed at the default `CLIMATE_ACTION_OFF` regardless of mode and temperature. Home Assistant now correctly shows `Cooling`, `Heating`, `Idle`, etc.
- **Preset Reset Mask When Changing Fan Mode (CNT)** — The fan mode change handler used mask `0xF0` to reset the preset, which did not clear the Auto Comfort bit (`0x20`). Changed to `0xD0` to match the preset handler.
- **Deprecated `traits()` Registration Removed** — Custom fan modes and presets were registered in both `traits()` (deprecated) and `setup()` (new API) as a workaround. Testing confirmed the `setup()`-only path works correctly on ESPHome 2026.5.1, so the deprecated `traits()` calls were removed — no more deprecation warnings during build.

### ℹ️ Known Limitations

- **nanoe-G is not supported on CZ-TACG1 (CNT)** — The CN-CNT serial protocol does not expose nanoe-G; pressing the button on the IR remote produces no change in the UART data stream. The Comfort Cloud API includes nanoe-G (NanoeMode values `3`/ModeG and `4`/All), which suggests it may be reachable via the DNSK-P11 WLAN protocol, but this is unconfirmed and not implemented.

## [2.6.0] – 2026-05-24

### ⚠️ Breaking Changes

- **ESPHome 2026.4+ required** — `custom_fan_mode` and `custom_preset` are now private members on the Climate class, and their getters return `StringRef` instead of `optional<std::string>`. If you read them in YAML lambdas, migrate:
  - `.custom_fan_mode.has_value()` → `!id(...).get_custom_fan_mode().empty()`
  - `.custom_fan_mode.value()` → `.get_custom_fan_mode()` (returns `StringRef`, compares directly with `"..."`)
  - `.custom_preset.has_value()` → `!id(...).get_custom_preset().empty()`
  - `.custom_preset.value()` → `.get_custom_preset()` (returns `StringRef`, compares directly with `"..."`)
  - **Do NOT compare with `nullptr`** — a `StringRef` vs `nullptr` comparison crashes (Load access fault).
- **Clear the ESPHome build cache after migrating** — Click **Clean Build Files** in the ESPHome dashboard (three-dot menu on your device) before compiling. Otherwise ESPHome may use a cached old version of the component and custom fan modes / presets won't work correctly.

### ✨ New Features

- **Inside Temperature Sensor** ([#184](https://github.com/DomiStyle/esphome-panasonic-ac/issues/184)) — New optional `inside_temperature` sensor that exposes the AC's internal temperature reading as a standalone entity for automations, dashboards and history graphs.
- **Defrost Sensor** ([#173](https://github.com/DomiStyle/esphome-panasonic-ac/pull/173)) — New optional `defrost_sensor` (binary sensor) that reports when the AC unit is defrosting. Supported on both CNT and WLAN.

### 🐛 Bug Fixes

- **Deprecated ClimateTraits Setters** ([#183](https://github.com/DomiStyle/esphome-panasonic-ac/issues/183), [#187](https://github.com/DomiStyle/esphome-panasonic-ac/pull/187)) — Registered `set_supported_custom_fan_modes()` and `set_supported_custom_presets()` in `setup()` on the Climate entity (new API). The deprecated `traits()` registration was kept alongside it because `ClimateCall::set_fan_mode()` did not find `setup()`-only modes on ESPHome 2026.5.x (both later removed in 2.6.1).
- **Temperature Offset Now Works on WLAN** ([#178](https://github.com/DomiStyle/esphome-panasonic-ac/pull/178)) — `current_temperature_offset` used to be CNT-only; it is now part of the common schema and works on both CNT and WLAN.
- **Econavi Toggle Destroying Preset & NanoeX State (CNT)** — `on_econavi_change` overwrote the entire shared byte instead of toggling only the econavi bit; turning econavi on reset the preset to Normal and disabled nanoeX.
- **NanoeX Toggle Destroying Econavi State (CNT)** — `on_nanoex_change` cleared the upper nibble of the shared byte, disabling econavi whenever nanoeX was toggled.
- **`determine_action()` in HEAT_COOL Mode** — In auto mode the action never returned `IDLE` and showed `COOLING` even below the target. Reworked with a proper deadband: `COOLING` above target+tolerance, `HEATING` below target-tolerance, `IDLE` in between.
- **WLAN Report Parser Out-of-Bounds** — Added a bounds check in the key-value pair loop to prevent out-of-bounds reads from malformed packets.
- **CNT Poll Response Missing Bounds Check** — `set_data()` accessed `rx_buffer_` indices up to 30 without verifying the packet size; a malformed short packet could cause out-of-bounds reads.
- **Unbounded Receive Buffer Growth (OOM)** — `rx_buffer_` had no size limit after moving from a fixed array to `std::vector`; UART noise could grow it indefinitely and crash the ESP32. Added a 256-byte limit with overflow protection.
- **Uninitialized Members** — The swing states (`vertical_swing_state_` / `horizontal_swing_state_`, could skip the first swing update), the WLAN command pointer (`last_command_` / `last_command_length_`, crash risk if `handle_resend()` fired first) and the timer members (`last_read_` / `last_packet_received_` / `init_time_` / `last_packet_sent_`) now all start from sane defaults.
- **`log_packet` Unnecessary Vector Copy** — Changed the parameter from pass-by-value to `const &`, avoiding a heap allocation on every log call.

## [2.5.0] – 2026-01-12

### 🛠 HA Compatibility

- **`StringRef` for Select Options** — Updated for ESPHome 2026.1.0 compatibility.

## [2.4.0] – 2025-11-15

### 🛠 HA Compatibility

- **ESPHome 2025.11+ Compatibility** — Fixed the deprecation warnings on ESPHome 2025.11.1 and updated the code to compile cleanly on 2025.11+.

## [2.3.0] – 2025-07-20

### 🐛 Bug Fixes

- **Deprecated Schema Warnings** — Fixed deprecated schema warnings in `climate.py`.

## [2.2.0]

### ✨ New Features

- **Temperature Offsets** — Added `current_temperature_offset` and `outside_temperature_offset`, with the offset limits raised to ±15 °.

## [2.1.0]

### ✨ New Features

- **Buffered CNT Commands** — CNT commands are now buffered before sending to prevent race conditions.
- **Vertical Swing "swing" Mode** — Added the `swing` vertical swing mode.
