#pragma once

#include "esphome/components/climate/climate.h"
#include "esphome/components/climate/climate_mode.h"
#include "esppac.h"

namespace esphome {
namespace panasonic_ac {
namespace CNT {

static const uint8_t CTRL_HEADER = 0xF0;  // The header for control frames
static const uint8_t POLL_HEADER = 0x70;  // The header for the poll command

static const int POLL_INTERVAL = 5000;  // The interval at which to poll the AC
static const int CMD_INTERVAL = 250;    // The interval at which to send commands

static const uint8_t DEFAULT_COMMAND_RETRIES = 5;  // Resend an unconfirmed command this many times
static const uint32_t DEFAULT_COMMAND_CONFIRM_DELAY = 1000;  // Poll this long after a command to verify it
static const uint32_t MAX_COMMAND_AGE = 300000;    // Drop a command that could not be verified for this long
static const uint8_t MAX_CONFIRM_POLLS = 5;        // Fast polls after a command while the AC stays silent
static const size_t MAX_STALE_STATES = 8;          // Upper limit for stale_states_, only reached if polls keep failing

enum class ACState {
  Initializing,  // Before first query response is receive
  Ready,         // All done, ready to receive regular packets
};

class PanasonicACCNT : public PanasonicAC {
 public:
  void control(const climate::ClimateCall &call) override;

  void on_horizontal_swing_change(const StringRef &swing) override;
  void on_vertical_swing_change(const StringRef &swing) override;
  void on_nanoex_change(bool nanoex) override;
  void on_eco_change(bool eco) override;
  void on_econavi_change(bool eco) override;
  void on_mild_dry_change(bool mild_dry) override;

  void set_command_retries(uint8_t command_retries) { this->command_retries_ = command_retries; }

  void set_command_confirm_delay(uint32_t command_confirm_delay) {
    this->command_confirm_delay_ = command_confirm_delay;
  }

  void setup() override;
  void loop() override;

 protected:
  ACState state_ = ACState::Initializing;  // Stores the internal state of the AC, used during initialization

  // uint8_t data[10];
  std::vector<uint8_t> data = std::vector<uint8_t>(10);  // Stores the data received from the AC
  std::vector<uint8_t> cmd;                              // Used to build next command

  uint8_t command_retries_ = DEFAULT_COMMAND_RETRIES;  // Resends of an unconfirmed command, 0 = never verify
  uint32_t command_confirm_delay_ = DEFAULT_COMMAND_CONFIRM_DELAY;
  uint32_t pending_since_ = 0;                         // When pending_cmd_ was requested
  uint8_t confirm_polls_left_ = 0;                     // Fast polls to send until the AC answers one
  bool resend_queued_ = false;                         // cmd is a resend of pending_cmd_, not a new request
  std::vector<uint8_t> pending_cmd_;                   // Last command sent, kept until the AC confirms it
  std::vector<std::vector<uint8_t>> stale_states_;     // States the AC reports if it missed pending_cmd_
  uint8_t cmd_resends_ = 0;                            // Resends of pending_cmd_ so far
  bool polled_since_cmd_ = false;                      // A poll was sent after pending_cmd_

  const std::vector<uint8_t> &cmd_base_() const;  // The state a new command starts from
  bool cmd_in_flight_() const;                    // Is a command queued or unconfirmed?
  bool is_stale_state_(const std::vector<uint8_t> &state) const;
  void add_stale_state_(const std::vector<uint8_t> &state);
  void track_cmd_();
  void expire_cmd_();
  void verify_cmd_();
  void clear_pending_cmd_();

  void handle_poll();
  void handle_cmd();

  void set_data(bool set);

  void send_command(std::vector<uint8_t> command, CommandType type, uint8_t header);
  void send_packet(const std::vector<uint8_t> &command, CommandType type);

  bool verify_packet();
  void handle_packet();
};

}  // namespace CNT
}  // namespace panasonic_ac
}  // namespace esphome
