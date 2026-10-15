// Ducati V2 integration; see the repository's PolyForm Noncommercial license.
#pragma once

#include <cstdint>

struct MotionVector {
  float x = 0.0F;
  float y = 0.0F;
  float z = 0.0F;
};

struct MotionImuSample {
  MotionVector accel_mps2;
  MotionVector gyro_dps;
  float heading_rate_dps = 0.0F;
  std::uint64_t sample_ms = 0;
  bool valid = false;
};
