#include "SolarCalculatorModule.h"

#include <SolarCalculator.h>
#include <time.h>

#include "Config.h"

void SolarCalculatorModule::setLocation(float latitude, float longitude) {
  _latitude = latitude;
  _longitude = longitude;
}

bool SolarCalculatorModule::hasValidTime() {
  return time(nullptr) >= static_cast<time_t>(MIN_VALID_EPOCH);
}

bool SolarCalculatorModule::update() {
  if (!hasValidTime()) return false;

  double azimuth = 0.0;
  double elevation = 0.0;
  calcHorizontalCoordinates(static_cast<unsigned long>(time(nullptr)), _latitude, _longitude, azimuth, elevation);

  // Convert from [0, 360) clockwise from North to (-180, 180], positive towards East.
  const double wrapped = azimuth > 180.0 ? azimuth - 360.0 : azimuth;

  _targetAzimuth = constrain(static_cast<float>(wrapped), AZ_LIMIT_MIN_DEG, AZ_LIMIT_MAX_DEG);
  _targetElevation = constrain(static_cast<float>(elevation), EL_LIMIT_MIN_DEG, EL_LIMIT_MAX_DEG);
  _night = elevation < NIGHT_ELEVATION_DEG;
  return true;
}
