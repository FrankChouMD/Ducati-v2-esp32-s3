#include "motion_lean_fusion.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
constexpr float kGravity = 9.80665F;
constexpr float kRad = 0.017453292519943295F;
int failures = 0;
#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << __LINE__ << " CHECK failed: " #condition << '\n'; ++failures; \
} } while (false)

// Screen faces up. Sensor +X is screen right, +Y is vehicle forward.
// Vehicle coordinates are forward/left/up, with positive roll toward the right.
MotionImuSample vehicle_sample(float roll_deg, float roll_rate_dps,
                               float clockwise_yaw_dps, float speed_mps,
                               std::uint64_t time_ms) {
  const float phi = roll_deg * kRad;
  const float yaw = clockwise_yaw_dps * kRad;
  MotionImuSample sample;
  sample.sample_ms = time_ms;
  sample.valid = true;
  // Specific force: earth-up gravity plus acceleration in a circular path.
  const float lateral = kGravity * std::sin(phi) - speed_mps * yaw * std::cos(phi);
  const float vertical = kGravity * std::cos(phi) + speed_mps * yaw * std::sin(phi);
  sample.accel_mps2 = {-lateral, 0.0F, vertical};
  sample.gyro_dps = {clockwise_yaw_dps * std::sin(phi), roll_rate_dps,
                     -clockwise_yaw_dps * std::cos(phi)};
  sample.heading_rate_dps = clockwise_yaw_dps;
  return sample;
}

std::uint64_t calibrate(MotionLeanFusion& fusion) {
  for (std::uint64_t t = 8; t <= 2104; t += 8) {
    auto sample = vehicle_sample(0, 0, 0, 0, t);
    // Realistic constant zero-rate errors should be removed on all three axes.
    sample.gyro_dps = {0.12F, -0.18F, 0.09F};
    fusion.update(sample);
    if (t < 2008) CHECK(!fusion.valid());
  }
  CHECK(fusion.calibrated());
  CHECK(fusion.valid());
  CHECK(std::abs(fusion.lean_deg()) < 0.1F);
  return 2104;
}

void test_static_left_right_and_mount() {
  MotionLeanFusion fusion;
  auto now = calibrate(fusion);
  for (float angle : {30.0F, -28.0F, 0.0F}) {
    for (int i = 0; i < 750; ++i) {
      now += 8;
      auto sample = vehicle_sample(angle, 0, 0, 0, now);
      sample.gyro_dps = {0.12F, -0.18F, 0.09F};
      fusion.update(sample);
    }
    CHECK(fusion.valid());
    CHECK(std::abs(fusion.lean_deg() - angle) < 0.2F);
  }

  // A backward-facing sensor normal and a 25-degree bracket pitch can both
  // be calibrated; the forward/left axes are derived from the upright sample.
  for (float sign : {1.0F, -1.0F}) {
    MotionLeanFusion mounted;
    const float pitch = 25.0F * kRad;
    MotionImuSample sample;
    sample.valid = true;
    sample.accel_mps2 = {0, sign * kGravity * std::sin(pitch),
                        sign * kGravity * std::cos(pitch)};
    for (std::uint64_t t = 8; t <= 2200; t += 8) {
      sample.sample_ms = t;
      mounted.update(sample);
    }
    CHECK(mounted.valid());
    CHECK(std::abs(mounted.lean_deg()) < 0.1F);
    const float roll = 35.0F * kRad;
    sample.accel_mps2 = {-kGravity * std::sin(roll),
        sign * kGravity * std::cos(roll) * std::sin(pitch),
        sign * kGravity * std::cos(roll) * std::cos(pitch)};
    for (std::uint64_t t = 2208; t <= 7000; t += 8) {
      sample.sample_ms = t;
      mounted.update(sample);
    }
    CHECK(mounted.valid());
    CHECK(std::abs(mounted.lean_deg() - 35.0F) < 0.2F);
  }
}

void test_turn_force_is_not_treated_as_gravity() {
  for (float sign : {1.0F, -1.0F}) {
    MotionLeanFusion fusion;
    auto now = calibrate(fusion);
    constexpr float speed = 20.0F;
    float final_angle = 0;
    // A one-second roll-in followed by 12 seconds of a balanced 30-degree turn.
    for (int i = 1; i <= 1625; ++i) {
      now += 8;
      const float angle = sign * std::min(30.0F, i * 0.24F);
      const float roll_rate = i <= 125 ? sign * 30.0F : 0.0F;
      const float yaw_dps = kGravity * std::tan(angle * kRad) / speed / kRad;
      auto sample = vehicle_sample(angle, roll_rate, yaw_dps, speed, now);
      sample.gyro_dps.x += 0.12F;
      sample.gyro_dps.y -= 0.18F;
      sample.gyro_dps.z += 0.09F;
      if (i % 50 == 1) fusion.set_speed(speed, now, true);
      fusion.update(sample);
      if (i > 375) {
        CHECK(fusion.valid());
        CHECK(std::abs(fusion.lean_deg() - sign * 30.0F) < 1.0F);
      }
      final_angle = fusion.lean_deg();
    }
    CHECK(std::abs(final_angle - sign * 30.0F) < 0.4F);

    // Loss of fresh speed must not turn a sustained bend into a false 0 degrees.
    // Brief gyro-only propagation expires, then the numeric readout is invalid.
    for (int i = 0; i < 700; ++i) {
      now += 8;
      const float yaw = sign * kGravity * std::tan(30.0F * kRad) / speed / kRad;
      fusion.update(vehicle_sample(sign * 30, 0, yaw, speed, now));
    }
    CHECK(!fusion.valid());
  }
}

void test_invalid_data_gaps_and_recalibration() {
  MotionLeanFusion fusion;
  auto now = calibrate(fusion);
  // A short longitudinal braking pulse is rejected by the correction gate.
  for (int i = 0; i < 100; ++i) {
    now += 8;
    auto sample = vehicle_sample(0, 0, 0, 0, now);
    sample.accel_mps2.y = -7.0F;
    sample.gyro_dps = {0.12F, -0.18F, 0.09F};
    fusion.update(sample);
  }
  CHECK(fusion.valid());
  CHECK(std::abs(fusion.lean_deg()) < 0.1F);
  auto bad = vehicle_sample(0, 0, 0, 0, now + 8);
  bad.accel_mps2.x = std::numeric_limits<float>::quiet_NaN();
  fusion.update(bad);
  CHECK(!fusion.valid());
  CHECK(!fusion.calibrated());
  now = calibrate(fusion);
  fusion.update(vehicle_sample(0, 0, 0, 0, now - 8));
  CHECK(fusion.valid());  // Out-of-order callbacks do not integrate backwards.
  fusion.update(vehicle_sample(0, 0, 0, 0, now + 200));
  CHECK(!fusion.valid());
  fusion.reset();
  CHECK(!fusion.valid());
  calibrate(fusion);
}

void test_zero_requires_quiet_stationary_samples_and_valid_axis() {
  MotionLeanFusion moving;
  for (std::uint64_t t = 8; t <= 4000; t += 8) {
    moving.set_speed(15.0F, t, true);
    moving.update(vehicle_sample(0, 0, 0, 0, t));
  }
  CHECK(!moving.valid());
  CHECK(!moving.calibrated());
  MotionLeanFusion vertical_axis(1);
  for (std::uint64_t t = 8; t <= 2400; t += 8) {
    auto sample = vehicle_sample(0, 0, 0, 0, t);
    sample.accel_mps2 = {kGravity, 0, 0};
    vertical_axis.update(sample);
  }
  CHECK(!vertical_axis.valid());
  MotionLeanFusion alternate_axis(2);
  calibrate(alternate_axis);
}

void test_gentle_turn_does_not_become_stationary_after_speed_expires() {
  MotionLeanFusion fusion;
  auto now = calibrate(fusion);
  constexpr float speed = 40.0F;
  constexpr float angle = 5.0F;
  const float yaw = kGravity * std::tan(angle * kRad) / speed / kRad;
  CHECK(yaw < 1.5F);  // Gyro magnitude alone would look quiet.
  for (int i = 0; i < 1250; ++i) {
    now += 8;
    if (i < 625 && i % 50 == 0) fusion.set_speed(speed, now, true);
    auto sample = vehicle_sample(angle, 0, yaw, speed, now);
    sample.gyro_dps.x += 0.12F;
    sample.gyro_dps.y -= 0.18F;
    sample.gyro_dps.z += 0.09F;
    fusion.update(sample);
  }
  CHECK(!fusion.valid());
  fusion.request_zero();  // Parked rider can re-zero after losing phone speed.
  for (int i = 0; i < 275; ++i) {
    now += 8;
    fusion.update(vehicle_sample(0, 0, 0, 0, now));
  }
  CHECK(fusion.valid());
}
}  // namespace

int main() {
  test_static_left_right_and_mount();
  test_turn_force_is_not_treated_as_gravity();
  test_invalid_data_gaps_and_recalibration();
  test_zero_requires_quiet_stationary_samples_and_valid_axis();
  test_gentle_turn_does_not_become_stationary_after_speed_expires();
  if (failures) return EXIT_FAILURE;
  std::cout << "Lean fusion: calibration, both turn directions, force compensation, stale speed and faults passed\n";
  return EXIT_SUCCESS;
}
