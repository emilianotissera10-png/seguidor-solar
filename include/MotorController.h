#pragma once

#include <Arduino.h>
#include <atomic>

// Non-blocking closed-loop FSM for one 28BYJ-48 stepper driven by a ULN2003.
// update()/stop() run in the control task and tick() runs in the motor task.
class MotorController {
 public:
  MotorController(uint8_t in1, uint8_t in2, uint8_t in3, uint8_t in4, bool inverted);

  void begin();
  void update(float actualDeg, float targetDeg);
  void stop();
  void tick();

 private:
  enum class State : uint8_t { Idle, MovingForward, MovingBackward };

  void writeCoils(uint8_t pattern);

  const uint8_t _pins[4];
  const bool _inverted;
  std::atomic<State> _state{State::Idle};
  uint8_t _phase = 0;
  bool _energized = false;
};
