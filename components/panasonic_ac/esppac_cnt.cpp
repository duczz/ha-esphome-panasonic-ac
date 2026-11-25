#include "esppac_cnt.h"
#include "esppac_commands_cnt.h"

#include "esphome/core/log.h"

namespace esphome {
namespace panasonic_ac {
namespace CNT {

static const char *const TAG = "panasonic_ac.cz_tacg1";

static climate::ClimateMode determine_mode(uint8_t mode) {
  uint8_t nib1 = (mode >> 4) & 0x0F;  // Left nib for mode
  uint8_t nib2 = (mode >> 0) & 0x0F;  // Right nib for power state

  if (nib2 == 0x00)
    return climate::CLIMATE_MODE_OFF;

  switch (nib1) {
    case 0x00:  // Auto
      return climate::CLIMATE_MODE_HEAT_COOL;
    case 0x03:  // Cool
      return climate::CLIMATE_MODE_COOL;
    case 0x04:  // Heat
      return climate::CLIMATE_MODE_HEAT;
    case 0x02:  // Dry
      return climate::CLIMATE_MODE_DRY;
    case 0x06:  // Fan only
      return climate::CLIMATE_MODE_FAN_ONLY;
    default:
      ESP_LOGW(TAG, "Received unknown climate mode");
      return climate::CLIMATE_MODE_OFF;
  }
}

static const char *determine_fan_speed(uint8_t speed) {
  switch (speed) {
    case 0xA0:  // Auto
      return "Automatic";
    case 0x30:  // 1
      return "1";
    case 0x40:  // 2
      return "2";
    case 0x50:  // 3
      return "3";
    case 0x60:  // 4
      return "4";
    case 0x70:  // 5
      return "5";
    default:
      ESP_LOGW(TAG, "Received unknown fan speed");
      return "Unknown";
  }
}

static const char *determine_vertical_swing(uint8_t swing) {
  uint8_t nib = (swing >> 4) & 0x0F;  // Left nib for vertical swing

  switch (nib) {
    case 0x0E:
      return "swing";
    case 0x0F:
      return "auto";
    case 0x01:
      return "up";
    case 0x02:
      return "up_center";
    case 0x03:
      return "center";
    case 0x04:
      return "down_center";
    case 0x05:
      return "down";
    case 0x00:
      return "unsupported";
    default:
      ESP_LOGW(TAG, "Received unknown vertical swing mode: 0x%02X", nib);
      return "Unknown";
  }
}

static const char *determine_horizontal_swing(uint8_t swing) {
  uint8_t nib = (swing >> 0) & 0x0F;  // Right nib for horizontal swing

  switch (nib) {
    case 0x0D:
      return "auto";
    case 0x09:
      return "left";
    case 0x0A:
      return "left_center";
    case 0x06:
      return "center";
    case 0x0B:
      return "right_center";
    case 0x0C:
      return "right";
    case 0x00:
      return "unsupported";
    default:
      ESP_LOGW(TAG, "Received unknown horizontal swing mode");
      return "Unknown";
  }
}

static const char *determine_preset(uint8_t preset) {
  if (preset & 0x20)
    return "Auto Comfort";

  switch (preset & 0x0F) {
    case 0x02:
      return "Powerful";
    case 0x04:
      return "Quiet";
    case 0x00:
      return "Normal";
    default:
      ESP_LOGW(TAG, "Received unknown preset");
      return "Normal";
  }
}

static bool determine_preset_nanoex(uint8_t preset) {
  uint8_t nib = (preset >> 4) & 0x04;  // Left nib for nanoex

  if (nib == 0x04)
    return true;
  else if (nib == 0x00)
    return false;
  else {
    ESP_LOGW(TAG, "Received unknown nanoex value");
    return false;
  }
}

static bool determine_eco(uint8_t value) {
  if (value == 0x40)
    return true;
  else if (value == 0x00)
    return false;
  else {
    ESP_LOGW(TAG, "Received unknown eco value");
    return false;
  }
}

static bool determine_econavi(uint8_t value) {
  uint8_t nib = value & 0x10;

  if (nib == 0x10)
    return true;
  else if (nib == 0x00)
    return false;
  else {
    ESP_LOGW(TAG, "Received unknown econavi value");
    return false;
  }
}

static bool determine_mild_dry(uint8_t value) {
  if (value == 0x7F)
    return true;
  else if (value == 0x80)
    return false;
  else {
    ESP_LOGW(TAG, "Received unknown mild dry value");
    return false;
  }
}

// Compare the bytes we control: mode, target temp, mild dry, fan, swing, preset/nanoex/econavi and eco.
// The remaining bytes are unknown or status and may change on their own.
static bool is_same_state(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b) {
  for (uint8_t i : {0, 1, 2, 3, 4, 5, 8}) {
    if (a[i] != b[i])
      return false;
  }
  return true;
}

static uint16_t determine_power_consumption(uint8_t byte_28, uint8_t byte_29, uint8_t offset) {
  return (uint16_t) (byte_28 + (byte_29 * 256)) - offset;
}

void PanasonicACCNT::setup() {
  PanasonicAC::setup();

  ESP_LOGD(TAG, "Using CZ-TACG1 protocol via CN-CNT");
}

void PanasonicACCNT::loop() {
  PanasonicAC::read_data();

  if (millis() - this->last_read_ > READ_TIMEOUT &&
      !this->rx_buffer_.empty())  // Check if our read timed out and we received something
  {
    log_packet(this->rx_buffer_);

    if (!verify_packet())  // Verify length, header, counter and checksum
      return;

    this->waiting_for_response_ = false;
    this->last_packet_received_ = millis();  // Set the time at which we received our last packet

    handle_packet();

    this->rx_buffer_.clear();  // Reset buffer
  }
  expire_cmd_();
  handle_cmd();
  handle_poll();  // Handle sending poll packets
}

/*
 * ESPHome control request
 */

void PanasonicACCNT::control(const climate::ClimateCall &call) {
  if (this->state_ != ACState::Ready)
    return;

  if (this->cmd.empty()) {
    ESP_LOGV(TAG, "Copying data to cmd");
    this->cmd = this->cmd_base_();
  }

  if (call.get_mode().has_value()) {
    ESP_LOGV(TAG, "Requested mode change");

    switch (*call.get_mode()) {
      case climate::CLIMATE_MODE_COOL:
        this->cmd[0] = 0x34;
        break;
      case climate::CLIMATE_MODE_HEAT:
        this->cmd[0] = 0x44;
        break;
      case climate::CLIMATE_MODE_DRY:
        this->cmd[0] = 0x24;
        break;
      case climate::CLIMATE_MODE_HEAT_COOL:
        this->cmd[0] = 0x04;
        break;
      case climate::CLIMATE_MODE_FAN_ONLY:
        this->cmd[0] = 0x64;
        break;
      case climate::CLIMATE_MODE_OFF:
        this->cmd[0] = this->cmd[0] & 0xF0;  // Strip right nib to turn AC off
        break;
      default:
        ESP_LOGV(TAG, "Unsupported mode requested");
        break;
    }
  }

  if (call.get_target_temperature().has_value()) {
    ESP_LOGV(TAG, "Requested target temp change to %.2f, %.2f including offset", *call.get_target_temperature(), *call.get_target_temperature() - this->current_temperature_offset_);
    this->cmd[1] = (*call.get_target_temperature() - this->current_temperature_offset_) / TEMPERATURE_STEP;
  }

  if (call.has_custom_fan_mode()) {
    ESP_LOGV(TAG, "Requested fan mode change");

    // The published preset lags behind while a command is unconfirmed
    if (this->get_custom_preset() != "Normal" ||
        (!this->pending_cmd_.empty() && StringRef(determine_preset(this->cmd[5])) != "Normal")) {
      ESP_LOGV(TAG, "Resetting preset");
      this->cmd[5] = (this->cmd[5] & 0xD0);  // Clear preset nibble + Auto Comfort bit
    }

    const auto fanMode = call.get_custom_fan_mode();

    if (fanMode == "Automatic")
      this->cmd[3] = 0xA0;
    else if (fanMode == "1")
      this->cmd[3] = 0x30;
    else if (fanMode == "2")
      this->cmd[3] = 0x40;
    else if (fanMode == "3")
      this->cmd[3] = 0x50;
    else if (fanMode == "4")
      this->cmd[3] = 0x60;
    else if (fanMode == "5")
      this->cmd[3] = 0x70;
    else {
      ESP_LOGV(TAG, "Unsupported fan mode requested");
    }
  }

  if (call.get_swing_mode().has_value()) {
    ESP_LOGV(TAG, "Requested swing mode change");

    switch (*call.get_swing_mode()) {
      case climate::CLIMATE_SWING_BOTH:
        this->cmd[4] = 0xFD;
        break;
      case climate::CLIMATE_SWING_OFF:
        this->cmd[4] = 0x36;  // Reset both to center
        break;
      case climate::CLIMATE_SWING_VERTICAL:
        this->cmd[4] = 0xF6;  // Swing vertical, horizontal center
        break;
      case climate::CLIMATE_SWING_HORIZONTAL:
        this->cmd[4] = 0x3D;  // Swing horizontal, vertical center
        break;
      default:
        ESP_LOGV(TAG, "Unsupported swing mode requested");
        break;
    }
  }

  if (call.has_custom_preset()) {
    ESP_LOGV(TAG, "Requested preset change");

    const auto preset = call.get_custom_preset();

    if (preset == "Normal")
      this->cmd[5] = (this->cmd[5] & 0xD0);          // Clear nibble + Auto Comfort bit
    else if (preset == "Powerful")
      this->cmd[5] = (this->cmd[5] & 0xD0) + 0x02;   // Clear nibble + Auto Comfort, set Powerful
    else if (preset == "Quiet")
      this->cmd[5] = (this->cmd[5] & 0xD0) + 0x04;   // Clear nibble + Auto Comfort, set Quiet
    else if (preset == "Auto Comfort")
      this->cmd[5] = (this->cmd[5] & 0xD0) + 0x20;   // Clear nibble, set Auto Comfort
    else {
      ESP_LOGV(TAG, "Unsupported preset requested");
    }
  }
}

/*
 * Set the data array to the fields
 */
void PanasonicACCNT::set_data(bool set) {
  this->mode = determine_mode(this->data[0]);
  this->set_custom_fan_mode_(determine_fan_speed(this->data[3]));

  StringRef verticalSwing(determine_vertical_swing(this->data[4]));
  StringRef horizontalSwing(determine_horizontal_swing(this->data[4]));

  const char *preset = determine_preset(this->data[5]);

  // Selects and switches ignore a change to the state they already show. Show the requested state while a command
  // is in flight, otherwise reverting it would be ignored and the retry would still apply it.
  const std::vector<uint8_t> &shown =
      !this->cmd_in_flight_() ? this->data : (this->cmd.empty() ? this->pending_cmd_ : this->cmd);

  bool nanoex = determine_preset_nanoex(shown[5]);
  bool eco = determine_eco(shown[8]);
  bool econavi = determine_econavi(shown[5]);
  bool mildDry = determine_mild_dry(shown[2]);

  this->update_target_temperature(this->data[1]);

  if (set) {
    if (this->rx_buffer_.size() >= 13) {
      this->operation_state_ = this->rx_buffer_[12];  // Byte 12: compressor/operation state (see determine_action)
      this->operation_state_valid_ = true;
    }

    if (this->rx_buffer_.size() < 23) {
      ESP_LOGW(TAG, "Poll response too short for extended data");
    } else {
      if (!this->has_valid_external_temperature_sensor()) {
        if (this->rx_buffer_[18] != 0x80)
          this->update_current_temperature((int8_t) this->rx_buffer_[18]);
        else if (this->rx_buffer_[21] != 0x80)
          this->update_current_temperature((int8_t) this->rx_buffer_[21]);
        else {
          ESP_LOGV(TAG, "Current temperature is not supported");
        }
      }

      if (this->outside_temperature_sensor_ != nullptr) {
        if (this->rx_buffer_[19] != 0x80)
          this->update_outside_temperature((int8_t) this->rx_buffer_[19]);
        else if (this->rx_buffer_[22] != 0x80)
          this->update_outside_temperature((int8_t) this->rx_buffer_[22]);
        else {
          ESP_LOGV(TAG, "Outside temperature is not supported");
        }
      }
    }

    if (this->current_power_consumption_sensor_ != nullptr && this->rx_buffer_.size() >= 31) {
      uint16_t power_consumption = determine_power_consumption(
          this->rx_buffer_[28], this->rx_buffer_[29], this->rx_buffer_[30]);
      this->update_current_power_consumption(power_consumption);
    }

    if (this->rx_buffer_.size() >= 15) {
      this->defrost_active_ = (this->rx_buffer_[14] == 0x02);
      update_defrost(this->defrost_active_);  // update_defrost is a no-op if no defrost sensor is configured
    }
  }

  if (verticalSwing == "auto" && horizontalSwing == "auto")
    this->swing_mode = climate::CLIMATE_SWING_BOTH;
  else if (verticalSwing == "auto")
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  else if (horizontalSwing == "auto")
    this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
  else
    this->swing_mode = climate::CLIMATE_SWING_OFF;

  this->update_swing_vertical(StringRef(determine_vertical_swing(shown[4])));
  this->update_swing_horizontal(StringRef(determine_horizontal_swing(shown[4])));

  this->set_custom_preset_(preset);

  this->update_nanoex(nanoex);
  this->update_eco(eco);
  this->update_econavi(econavi);
  this->update_mild_dry(mildDry);
}

/*
 * Send a command, attaching header, packet length and checksum
 */
void PanasonicACCNT::send_command(std::vector<uint8_t> command, CommandType type, uint8_t header = CNT::CTRL_HEADER) {
  uint8_t length = command.size();
  command.insert(command.begin(), header);
  command.insert(command.begin() + 1, length);

  uint8_t checksum = 0;

  for (uint8_t i : command)
    checksum -= i;  // Add to checksum

  command.push_back(checksum);

  send_packet(command, type);  // Actually send the constructed packet
}

/*
 * Send a raw packet, as is
 */
void PanasonicACCNT::send_packet(const std::vector<uint8_t> &packet, CommandType type) {
  this->last_packet_sent_ = millis();  // Save the time when we sent the last packet

  if (type != CommandType::Response)     // Don't wait for a response for responses
    this->waiting_for_response_ = true;  // Mark that we are waiting for a response

  write_array(packet);       // Write to UART
  log_packet(packet, true);  // Write to log
}

/*
 * Loop handling
 */

void PanasonicACCNT::handle_poll() {
  const uint32_t now = millis();
  // Verify a command early instead of waiting for the next regular poll
  const bool confirm = this->confirm_polls_left_ > 0 && now - this->last_packet_sent_ > CMD_INTERVAL &&
                       now - this->last_packet_sent_ >= this->command_confirm_delay_;

  if (confirm || now - this->last_packet_sent_ > POLL_INTERVAL) {
    ESP_LOGV(TAG, "Polling AC");
    send_command(CMD_POLL, CommandType::Normal, POLL_HEADER);
    this->polled_since_cmd_ = true;
    if (this->confirm_polls_left_ > 0)
      this->confirm_polls_left_--;
  }
}

void PanasonicACCNT::handle_cmd() {
  if (!this->cmd.empty() && millis() - this->last_packet_sent_ > CMD_INTERVAL) {
    ESP_LOGV(TAG, "Sending Command");
    this->track_cmd_();
    send_command(this->cmd, CommandType::Normal, CTRL_HEADER);
    this->cmd.clear();
  }
}

/*
 * Command verification, the AC does not acknowledge commands so a poll response is the only confirmation
 */

const std::vector<uint8_t> &PanasonicACCNT::cmd_base_() const {
  // Start from the unconfirmed command, so a second change does not undo it
  return this->pending_cmd_.empty() ? this->data : this->pending_cmd_;
}

bool PanasonicACCNT::cmd_in_flight_() const {
  return this->command_retries_ > 0 && (!this->cmd.empty() || !this->pending_cmd_.empty());
}

bool PanasonicACCNT::is_stale_state_(const std::vector<uint8_t> &state) const {
  for (const auto &stale : this->stale_states_) {
    if (is_same_state(state, stale))
      return true;
  }
  return false;
}

void PanasonicACCNT::add_stale_state_(const std::vector<uint8_t> &state) {
  if (this->is_stale_state_(state))
    return;
  if (this->stale_states_.size() >= MAX_STALE_STATES)
    this->stale_states_.erase(this->stale_states_.begin());
  this->stale_states_.push_back(state);
}

void PanasonicACCNT::track_cmd_() {
  if (this->command_retries_ == 0)
    return;

  if (this->cmd != this->pending_cmd_) {
    // The AC may have missed both commands or only this one
    this->add_stale_state_(this->data);
    if (!this->pending_cmd_.empty())
      this->add_stale_state_(this->pending_cmd_);
  }

  if (!this->resend_queued_ || this->cmd != this->pending_cmd_) {  // A new request, not a resend
    this->pending_cmd_ = this->cmd;
    this->pending_since_ = millis();
    this->cmd_resends_ = 0;
  }

  this->resend_queued_ = false;
  this->polled_since_cmd_ = false;  // Ignore the response to a poll sent before this command
  this->confirm_polls_left_ = MAX_CONFIRM_POLLS;
}

void PanasonicACCNT::verify_cmd_() {
  if (this->pending_cmd_.empty())
    return;

  // The AC answers a control frame it received with its new state. Everything else, including a match that can just
  // as well be the state from before the command, needs a poll sent after the command.
  if (!this->polled_since_cmd_ &&
      (!is_same_state(this->data, this->pending_cmd_) || this->is_stale_state_(this->pending_cmd_)))
    return;

  this->confirm_polls_left_ = 0;  // The AC answered, back to the regular poll interval

  if (!this->cmd.empty())  // A newer change is about to replace the pending command
    return;

  if (is_same_state(this->data, this->pending_cmd_)) {
    if (this->cmd_resends_ > 0) {
      ESP_LOGI(TAG, "Command confirmed after %u resend(s)", this->cmd_resends_);
    }
    this->clear_pending_cmd_();
    return;
  }

  if (!this->is_stale_state_(this->data)) {
    // Somebody else changed the AC, eg. via the IR remote, a resend would override that
    ESP_LOGI(TAG, "AC state changed externally while a command was pending, dropping retry");
    this->clear_pending_cmd_();
    return;
  }

  if (this->cmd_resends_ >= this->command_retries_) {
    ESP_LOGE(TAG, "AC did not apply last command after %u resend(s), giving up", this->cmd_resends_);
    this->clear_pending_cmd_();
    return;
  }

  this->cmd_resends_++;
  ESP_LOGW(TAG, "AC did not apply last command, resending (%u/%u)", this->cmd_resends_, this->command_retries_);
  this->stale_states_.assign(1, this->data);  // From here on only this exact state counts as not applied
  this->cmd = this->pending_cmd_;
  this->resend_queued_ = true;
}

void PanasonicACCNT::expire_cmd_() {
  if (this->pending_cmd_.empty() || !this->cmd.empty() || millis() - this->pending_since_ < MAX_COMMAND_AGE)
    return;

  // The AC state is unknown for too long, applying the command later would come as a surprise
  ESP_LOGE(TAG, "AC did not answer for too long, dropping last command");
  this->clear_pending_cmd_();
}

void PanasonicACCNT::clear_pending_cmd_() {
  this->pending_cmd_.clear();
  this->stale_states_.clear();
  this->cmd_resends_ = 0;
  this->confirm_polls_left_ = 0;
}

/*
 * Packet handling
 */

bool PanasonicACCNT::verify_packet() {
  if (this->rx_buffer_.size() < 12) {
    ESP_LOGW(TAG, "Dropping invalid packet (length)");

    this->rx_buffer_.clear();  // Reset buffer
    return false;
  }

  // Check if header matches
  if (this->rx_buffer_[0] != CTRL_HEADER && this->rx_buffer_[0] != POLL_HEADER) {
    ESP_LOGW(TAG, "Dropping invalid packet (header)");

    this->rx_buffer_.clear();  // Reset buffer
    return false;
  }

  // Packet length minus header, packet length and checksum
  if (this->rx_buffer_[1] != this->rx_buffer_.size() - 3) {
    ESP_LOGD(TAG, "Dropping invalid packet (length mismatch)");

    this->rx_buffer_.clear();  // Reset buffer
    return false;
  }

  uint8_t checksum = 0;

  for (uint8_t b : this->rx_buffer_) {
    checksum += b;
  }

  if (checksum != 0) {
    ESP_LOGD(TAG, "Dropping invalid packet (checksum)");

    this->rx_buffer_.clear();  // Reset buffer
    return false;
  }

  return true;
}

void PanasonicACCNT::handle_packet() {
  if (this->rx_buffer_[0] == POLL_HEADER) {
    this->data = std::vector<uint8_t>(this->rx_buffer_.begin() + 2, this->rx_buffer_.begin() + 12);

    this->verify_cmd_();
    this->set_data(true);
    this->action = determine_action();
    this->publish_state();

    if (this->state_ != ACState::Ready)
      this->state_ = ACState::Ready;  // Mark as ready after first poll
  } else {
    ESP_LOGD(TAG, "Received unknown packet");
  }
}

/*
 * Sensor handling
 */

void PanasonicACCNT::on_vertical_swing_change(const StringRef &swing) {
  if (this->state_ != ACState::Ready)
    return;

  ESP_LOGD(TAG, "Setting vertical swing position");

  if (this->command_retries_ > 0)  // Remember the request, so reverting it before the next poll is not ignored
    this->vertical_swing_state_ = this->vertical_swing_select_->active_index().value_or(~0UL);

  if (this->cmd.empty()) {
    ESP_LOGV(TAG, "Copying data to cmd");
    this->cmd = this->cmd_base_();
  }

  if (swing == "down")
    this->cmd[4] = (this->cmd[4] & 0x0F) + 0x50;
  else if (swing == "down_center")
    this->cmd[4] = (this->cmd[4] & 0x0F) + 0x40;
  else if (swing == "center")
    this->cmd[4] = (this->cmd[4] & 0x0F) + 0x30;
  else if (swing == "up_center")
    this->cmd[4] = (this->cmd[4] & 0x0F) + 0x20;
  else if (swing == "up")
    this->cmd[4] = (this->cmd[4] & 0x0F) + 0x10;
  else if (swing == "swing")
    this->cmd[4] = (this->cmd[4] & 0x0F) + 0xE0;
  else if (swing == "auto")
    this->cmd[4] = (this->cmd[4] & 0x0F) + 0xF0;
  else {
    ESP_LOGW(TAG, "Unsupported vertical swing position received");
    return;
  }
}

void PanasonicACCNT::on_horizontal_swing_change(const StringRef &swing) {
  if (this->state_ != ACState::Ready)
    return;

  ESP_LOGD(TAG, "Setting horizontal swing position");

  if (this->command_retries_ > 0)  // Remember the request, so reverting it before the next poll is not ignored
    this->horizontal_swing_state_ = this->horizontal_swing_select_->active_index().value_or(~0UL);

  if (this->cmd.empty()) {
    ESP_LOGV(TAG, "Copying data to cmd");
    this->cmd = this->cmd_base_();
  }

  if (swing == "left")
    this->cmd[4] = (this->cmd[4] & 0xF0) + 0x09;
  else if (swing == "left_center")
    this->cmd[4] = (this->cmd[4] & 0xF0) + 0x0A;
  else if (swing == "center")
    this->cmd[4] = (this->cmd[4] & 0xF0) + 0x06;
  else if (swing == "right_center")
    this->cmd[4] = (this->cmd[4] & 0xF0) + 0x0B;
  else if (swing == "right")
    this->cmd[4] = (this->cmd[4] & 0xF0) + 0x0C;
  else if (swing == "auto")
    this->cmd[4] = (this->cmd[4] & 0xF0) + 0x0D;
  else {
    ESP_LOGW(TAG, "Unsupported horizontal swing position received");
    return;
  }
}

void PanasonicACCNT::on_nanoex_change(bool state) {
  if (this->state_ != ACState::Ready)
    return;

  if (this->cmd.empty()) {
    ESP_LOGV(TAG, "Copying data to cmd");
    this->cmd = this->cmd_base_();
  }

  this->nanoex_state_ = state;

  if (state) {
    ESP_LOGV(TAG, "Turning nanoex on");
    this->cmd[5] |= 0x40;
  } else {
    ESP_LOGV(TAG, "Turning nanoex off");
    this->cmd[5] &= ~0x40;
  }
}

void PanasonicACCNT::on_eco_change(bool state) {
  if (this->state_ != ACState::Ready)
    return;

  if (this->cmd.empty()) {
    ESP_LOGV(TAG, "Copying data to cmd");
    this->cmd = this->cmd_base_();
  }

  this->eco_state_ = state;

  if (state) {
    ESP_LOGV(TAG, "Turning eco mode on");
    this->cmd[8] = 0x40;
  } else {
    ESP_LOGV(TAG, "Turning eco mode off");
    this->cmd[8] = 0x00;
  }
}

void PanasonicACCNT::on_econavi_change(bool state) {
  if (this->state_ != ACState::Ready)
    return;

  if (this->cmd.empty()) {
    ESP_LOGV(TAG, "Copying data to cmd");
    this->cmd = this->cmd_base_();
  }

  this->econavi_state_ = state;

  if (state) {
    ESP_LOGV(TAG, "Turning econavi mode on");
    this->cmd[5] |= 0x10;
  } else {
    ESP_LOGV(TAG, "Turning econavi mode off");
    this->cmd[5] &= ~0x10;
  }
}

void PanasonicACCNT::on_mild_dry_change(bool state) {
  if (this->state_ != ACState::Ready)
    return;

  if (this->cmd.empty()) {
    ESP_LOGV(TAG, "Copying data to cmd");
    this->cmd = this->cmd_base_();
  }

  this->mild_dry_state_ = state;

  if (state) {
    ESP_LOGV(TAG, "Turning mild dry on");
    this->cmd[2] = 0x7F;
  } else {
    ESP_LOGV(TAG, "Turning mild dry off");
    this->cmd[2] = 0x80;
  }
}

}  // namespace CNT
}  // namespace panasonic_ac
}  // namespace esphome
