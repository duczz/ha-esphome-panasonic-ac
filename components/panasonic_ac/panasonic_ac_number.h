#pragma once

#include "esphome/core/component.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace panasonic_ac {

class PanasonicACNumber : public number::Number, public Component {
 public:
  void control(float value) override {
    this->publish_state(value);
  }
};

}  // namespace panasonic_ac
}  // namespace esphome
