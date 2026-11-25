# Changelog

## [2.6.5] – 2026-11-25

A release for CN-CNT setups (CZ-TACG1): command confirmation with automatic resend, plus several bug fixes. WLAN (DNSK-P11) is unaffected.

### ✨ New Features

- **Command Confirmation (CNT)** — Every command is now kept until the AC confirms the requested state (for at most 5 minutes), and resent if it still reports the old one. If the AC reports a third state, for example because the IR remote was used, the retry is dropped and the external change wins. Resends, confirmations after a resend and giving up are visible at the default log level. Verified on a Panasonic CS-Z50UFEAW (outdoor unit CU-Z50UBEA) over CN-CNT: fan, vertical swing, nanoeX and preset changes were confirmed by the AC. Mode, target temperature, Econavi, mild dry and Eco were not changed in that test. It is on by default, `command_retries: 0` turns it off.
- **`command_retries` and `command_confirm_delay` (CNT)** — Two optional settings for the new command verification. `command_retries` (0 to 10, default `5`) sets how often an unconfirmed command is resent, `0` turns the verification off: each command is sent once and not checked. `command_confirm_delay` (`250ms` to `5s`, default `1s`) sets how long after a command the AC is polled to verify it, instead of waiting up to 5 s for the next regular poll. That poll is repeated up to 5 times while the AC stays silent.

### 🐛 Bug Fixes

- **Change After a Missed Command Undid It (CNT)** — If the AC missed a command and the next poll reported the old state, a following change was built from that old state and dropped the first one. It now starts from the unconfirmed command. Reverting a swing select or a switch before the AC confirmed the change is no longer ignored either.
- **CN-CNT: Valid Poll Reply Dropped When Noise Bytes Precede It** — With TX→RX crosstalk in the wiring (e.g. a level shifter echoing our own poll back as distorted bytes), the echo and the real reply ended up in one read buffer. `verify_packet()` then rejected the whole buffer (`Dropping invalid packet (length mismatch)`), so the component never reached the ready state. The receive path now skips leading bytes up to the first complete packet (header, matching length byte, zero checksum). A buffer that already is a complete packet is left untouched, and the skipped byte count is logged at `VERBOSE`. **Affected:** CN-CNT setups with leading noise before the reply; clean setups behave as before. Fixing the wiring is still the better cure. Checked offline against constructed packets, not yet on real hardware.
- **CN-CNT: Echo of Our Own Packet Shown as "Off, 0 °C"** — On some wirings the line returns the request we just sent, byte for byte, while the indoor unit is busy. The echoed poll (`70 0A 00 … 86`) is a well-formed packet, so it was decoded as mode off / 0 °C and the climate entity flipped for one or two poll intervals. The component now remembers the last transmitted packet and ignores an identical received one (logged at `DEBUG`). **Affected:** CN-CNT setups that echo; others behave as before. Not yet tested on real hardware.
- **CN-CNT: Quick Follow-up Command Reverted the First One** — A second `control()` call arriving after the first command was sent but before the AC's reply was processed (~100–180 ms) was built from the stale state and silently undid the first, e.g. `set_hvac_mode: heat` followed by `set_temperature` (as sent by the HomeKit bridge) switched the unit on and straight off again. After sending, the sent command is now stored as the current state; the next poll still overwrites it with the real one, so a rejected command is corrected. Not yet tested on real hardware.

### 📖 Documentation

- **External Temperature Sensor From Home Assistant** — README and `ac.yaml.example` now show how to feed a Home Assistant sensor into `current_temperature_sensor` (via a `homeassistant` platform sensor with an `id`; `entity_id` is not accepted there).

## [2.6.4] – 2026-11-21

A small bug-fix release. **No functional changes** — AC control, protocol handling and all entities behave exactly as in 2.6.3.

### 🐛 Bug Fixes

- **Build Fails with `logger: level: VERBOSE` on ESPHome 2026.11 and Newer** ([#4](https://github.com/duczz/ha-esphome-panasonic-ac/issues/4)) — Compiling failed with `'format_hex_pretty' was not declared in this scope` when the log level was `VERBOSE` or higher, for example when a package raised it. **Affected:** ESPHome 2026.11 and newer, and only at `VERBOSE` or higher; at the default level and on older ESPHome versions the build was fine. The `std::string` overload of `format_hex_pretty` lives in `esphome/core/alloc_helpers.h`, which ESPHome 2026.11 no longer includes via `helpers.h`. `log_packet()` now uses the stack-based `format_hex_pretty_to()`, available since ESPHome 2026.4 (the minimum for this fork), so it works on all supported versions.

### 🔧 Changed Log Format

- With `VERBOSE` logging the raw packet lines look slightly different: `TX: AA:BB:CC` instead of `TX: AA.BB.CC (3)` (separator `:`, no length suffix).

## [2.6.3] – 2026-09-21

A small maintenance release. **No functional changes** — AC control, protocol handling and all entities behave exactly as in 2.6.2.

### 🔧 Code Quality

- **Clean Builds (No More Compiler Warnings)** — Building the component no longer prints six `-Wempty-body` warnings ("suggest braces around empty body in an 'else' statement") in `esppac_cnt.cpp` and `esppac_wlan.cpp`. `ESP_LOGV` compiles to an empty statement at log level `INFO`, so GCC flagged the unbraced `else` branches. Braces added; behaviour unchanged.

### 📖 Documentation

- **Daily Energy Recipe** — README and `ac.yaml.example` now show how to get a `Total Daily Energy` sensor in kWh from `current_power_consumption` (`total_daily_energy` platform, `multiply: 0.001`, `time: homeassistant`). The power sensor needs an `id:` for this.
- **Supported Values** — New README section "Entities and supported values" listing what the climate entity and its selects/switches accept.
- **Sensor Filtering** — Documented how to use an external temperature sensor and that implausible readings from the AC are discarded automatically.
- **`refresh: always` Hint Fixed in `ac.yaml.example`** — The commented-out `refresh: always` line was indented inside `source:`, where ESPHome does not accept it. It now sits on the same level as `source:` and `components:`, so uncommenting it works. The README cache section points to it as an alternative to "Clean Build Files".
- **README Feature List** — Now also lists the compressor-based climate action and the Auto Comfort preset.
- **README & CHANGELOG Layout** — Table of Contents moved to the top, fork section slimmed down and pointing to the CHANGELOG, CHANGELOG restyled to the emoji-section schema.

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
