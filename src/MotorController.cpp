#include "MotorController.h"

#include "Config.h"

namespace {
// Half-step sequence, bit 0 = IN1 ... bit 3 = IN4.
const uint8_t kSequence[8] = {0b0001, 0b0011, 0b0010, 0b0110, 0b0100, 0b1100, 0b1000, 0b1001};
const uint8_t kPhaseMask = 7;
}  // namespace

MotorController::MotorController(uint8_t in1, uint8_t in2, uint8_t in3, uint8_t in4, bool inverted)
    : _pins{in1, in2, in3, in4}, _inverted(inverted) {}

void MotorController::begin() {
  for (uint8_t pin : _pins) {
    pinMode(pin, OUTPUT);
  }
  writeCoils(0);
}

void MotorController::update(float actualDeg, float targetDeg) {
  const float error = targetDeg - actualDeg;
  const float magnitude = fabsf(error);

  // The start threshold is wider than the stop threshold to prevent jitter.
  if (_state.load() == State::Idle) {
    if (magnitude <= DEADBAND_DEG) return;
  } else if (magnitude <= STOP_BAND_DEG) {
    _state.store(State::Idle);
    return;
  }
  _state.store(error > 0.0f ? State::MovingForward : State::MovingBackward);
}

void MotorController::stop() {
  _state.store(State::Idle);
}

void MotorController::tick() {
  const State state = _state.load();
  if (state == State::Idle) {
    // Release the coils once to avoid overheating at rest.
    if (_energized) {
      writeCoils(0);
      _energized = false;
    }
    return;
  }

  const bool increasePhase = (state == State::MovingForward) != _inverted;
  _phase = (_phase + (increasePhase ? 1 : kPhaseMask)) & kPhaseMask;
  writeCoils(kSequence[_phase]);
  _energized = true;
}

void MotorController::writeCoils(uint8_t pattern) {
  for (uint8_t i = 0; i < 4; i++) {
    digitalWrite(_pins[i], (pattern >> i) & 1 ? HIGH : LOW);
  }
}
