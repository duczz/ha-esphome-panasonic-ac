# Changelog

## 2.6.2 (2026-05-31)

### Added

- **External Temperature Sensor Fallback**: Added a fallback mechanism for the `current_temperature_sensor`. If the external sensor becomes unavailable (e.g., reports `NaN` due to an empty battery or connection loss), the integration will now gracefully fall back to using the AC's internal temperature sensor for its operations until the external sensor recovers. This applies to both CNT and WLAN protocols.
- **Live Temperature Offset Adjustments** (#179): You can now adjust the `current_temperature_offset` and `outside_temperature_offset` dynamically from Home Assistant without recompiling ESPHome! Use the new `current_temperature_offset_number` and `outside_temperature_offset_number` configuration variables to expose sliders in Home Assistant.

## 2.6.1 (2026-05-27)

### Added

- **Auto Comfort preset** (CNT): Added the Auto Comfort preset to the climate entity. Selecting it sends bit `0x20` in the preset byte; switching back to Normal/Powerful/Quiet clears it correctly via the `0xD0` mask. Reverse-engineered from UART logs by comparing the AC's response when the IR remote's Auto Comfort button was pressed.
- **Expanded `ac.yaml.example`**: Now includes substitutions, `esp-idf` framework, template selects for Fan Mode / Mode / Preset Mode with `on_state` sync lambdas, and a `refresh: always` hint for cache issues.

### Fixed

- **Fix CNT climate action always showing "Off"**: `determine_action()` was never called in the CNT poll handler — the action stayed at the default `CLIMATE_ACTION_OFF` regardless of mode and temperature. Home Assistant now correctly shows "Cooling", "Heating", "Idle" etc.
- **Fix preset reset mask when changing fan mode** (CNT): The fan mode change handler used mask `0xF0` to reset the preset, which did not clear the Auto Comfort bit (`0x20`). Changed to `0xD0` to match the preset handler.
- **Remove deprecated `traits()` registration**: Custom fan modes and presets were registered in both `traits()` (deprecated) and `setup()` (new API) as a workaround. Testing confirmed `setup()`-only works correctly in ESPHome 2026.5.1 — the deprecated `traits()` calls have been removed. No more deprecation warnings during build.

### Notes

- **nanoe-G is not supported on CZ-TACG1 (CNT)**: The CN-CNT serial protocol does not expose nanoe-G. Pressing the nanoe-G button on the IR remote produces no change in the UART data stream — the protocol byte is identical whether nanoe-G is active or not. The Comfort Cloud API includes nanoe-G (NanoeMode values `3`/ModeG and `4`/All), which suggests it may be accessible via the DNSK-P11 WLAN protocol (CN-WLAN), but this is unconfirmed and not implemented.

## 2.6.0 (2026-05-24)

### Breaking Changes

- **ESPHome 2026.4+ required**: `custom_fan_mode` and `custom_preset` are now private members on the Climate class. The getters return `StringRef` instead of `optional<std::string>`. If you access them in YAML lambdas, replace:
  - `.custom_fan_mode.has_value()` → `!.get_custom_fan_mode().empty()`
  - `.custom_fan_mode.value()` → `.get_custom_fan_mode()` (returns `StringRef`, can be compared with `"..."` directly)
  - `.custom_preset.has_value()` → `!.get_custom_preset().empty()`
  - `.custom_preset.value()` → `.get_custom_preset()` (returns `StringRef`, can be compared with `"..."` directly)
  - **Do NOT compare with `nullptr`** — `StringRef` comparison with `nullptr` causes a crash (Load access fault)
- **Clear ESPHome build cache after migrating**: Click **Clean Build Files** in the ESPHome dashboard (three-dot menu on your device) before compiling. Without this, ESPHome may use a cached old version of the component and custom fan modes / presets will not work correctly.

### Fixed

- **Fix deprecated ClimateTraits setters** (upstream [#183](https://github.com/DomiStyle/esphome-panasonic-ac/issues/183), [#187](https://github.com/DomiStyle/esphome-panasonic-ac/pull/187)): Added `set_supported_custom_fan_modes()` and `set_supported_custom_presets()` in `setup()` on the Climate entity (new API). The deprecated `traits()` registration is kept as well because `ClimateCall::set_fan_mode()` routing does not find modes registered via `setup()` alone in ESPHome 2026.5.x. Both registrations will remain until ESPHome fixes this or removes the deprecated API in 2026.11.0.
- **Fix temperature offset not working for WLAN variant** (upstream [#178](https://github.com/DomiStyle/esphome-panasonic-ac/pull/178)): `current_temperature_offset` was only available for CNT devices. It is now part of the common schema and works for both CNT and WLAN.
- **Fix econavi toggle destroying preset and nanoex state** (CNT): `on_econavi_change` overwrote the entire shared byte instead of toggling only the econavi bit. Turning econavi on would reset preset to Normal and disable nanoex.
- **Fix nanoex toggle destroying econavi state** (CNT): `on_nanoex_change` cleared the upper nibble of the shared byte, disabling econavi whenever nanoex was toggled.
- **Fix potential crash in WLAN report parser**: Added bounds check in the key-value pair loop to prevent out-of-bounds reads from malformed packets.
- **Fix uninitialized swing state members**: `vertical_swing_state_` and `horizontal_swing_state_` could have indeterminate values, potentially causing the first swing update to be skipped.
- **Fix uninitialized WLAN command pointer**: `last_command_` and `last_command_length_` were uninitialized, risking a crash if `handle_resend()` fired before any command was sent.
- **Fix unbounded receive buffer growth**: `rx_buffer_` had no size limit after migrating from a fixed array to `std::vector`. UART noise could grow the buffer indefinitely, causing OOM on the ESP32. Added a 256-byte limit with overflow protection.
- **Fix `determine_action()` logic for HEAT_COOL mode**: In auto mode, the action was never IDLE and showed COOLING even when below target. Reworked to use a proper deadband: COOLING when above target+tolerance, HEATING when below target-tolerance, IDLE in between.
- **Fix CNT poll response missing bounds check**: `set_data()` accessed `rx_buffer_` indices up to 30 without verifying packet size. A malformed short packet could cause out-of-bounds reads.
- **Fix `log_packet` unnecessary vector copy**: Changed parameter from pass-by-value to `const &`, avoiding a heap allocation on every log call.
- **Fix uninitialized timer members**: `last_read_`, `last_packet_received_`, `init_time_`, `last_packet_sent_` now default to 0.

### Added

- **Inside temperature sensor** (upstream [#184](https://github.com/DomiStyle/esphome-panasonic-ac/issues/184)): New optional `inside_temperature` sensor that exposes the AC's internal temperature reading as a standalone entity for use in automations, dashboards and history graphs.
- **Defrost sensor** (upstream [#173](https://github.com/DomiStyle/esphome-panasonic-ac/pull/173)): New optional `defrost_sensor` (binary sensor) that reports when the AC unit is in defrost mode. Supported on both CNT and WLAN variants.

## 2.5.0 (2026-01-12)

- Use `StringRef` for Select options (ESPHome 2026.1.0 compatibility)

## 2.4.0 (2025-11-15)

- Fix deprecation warnings with ESPHome 2025.11.1
- Update code to be compatible with ESPHome 2025.11+

## 2.3.0 (2025-07-20)

- Fix deprecated schema warnings in `climate.py`

## 2.2.0

- Add temperature offsets for current and outside temperature
- Increase offset limits to +/-15 degrees

## 2.1.0

- Buffer CNT commands before sending to prevent race conditions
- Add vertical swing mode "swing"
