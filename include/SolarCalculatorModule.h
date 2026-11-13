#pragma once

#include <Arduino.h>

// Computes the mechanical target angles from the system UTC time and the location.
class SolarCalculatorModule {
 public:
  void setLocation(float latitude, float longitude);

  // Returns false, keeping the previous results, if the system time was never set.
  bool update();

  static bool hasValidTime();

  float targetAzimuth() const { return _targetAzimuth; }
  float targetElevation() const { return _targetElevation; }
  bool isNight() const { return _night; }

 private:
  double _latitude = 0.0;
  double _longitude = 0.0;
  float _targetAzimuth = 0.0f;
  float _targetElevation = 0.0f;
  bool _night = false;
};
