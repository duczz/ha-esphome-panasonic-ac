# Changelog

## 2.6.0 (2026-05-24)

### Breaking Changes

- **ESPHome 2026.4+ required**: `custom_fan_mode` and `custom_preset` are now private members on the Climate class. If you access them in YAML lambdas, replace:
  - `.custom_fan_mode.has_value()` → `.get_custom_fan_mode() != nullptr`
  - `.custom_fan_mode.value()` → `.get_custom_fan_mode()`
  - `.custom_preset.has_value()` → `.get_custom_preset() != nullptr`
  - `.custom_preset.value()` → `.get_custom_preset()`

### Fixed

- **Fix deprecated ClimateTraits setters** (upstream [#183](https://github.com/DomiStyle/esphome-panasonic-ac/issues/183), [#187](https://github.com/DomiStyle/esphome-panasonic-ac/pull/187)): Moved `set_supported_custom_fan_modes()` and `set_supported_custom_presets()` from `traits()` to `setup()`, calling them on the Climate entity instead of ClimateTraits. Without this fix, compilation will fail starting with ESPHome 2026.11.0.
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
