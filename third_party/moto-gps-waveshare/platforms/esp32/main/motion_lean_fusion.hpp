// Ducati V2 integration; see the repository's PolyForm Noncommercial license.
// Original prototype: gyro gravity propagation with velocity-compensated
// accelerometer correction. Derivation, assumptions and sources: docs/LEAN_ANGLE.md.
#pragma once

#include "motion_imu_sample.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

// Sensor-axis pointing toward the RIGHT of the installed screen: +X/-X/+Y/-Y.
// This is an installation setting, not something gravity alone can identify.
#ifndef MOTO_IMU_SCREEN_RIGHT_AXIS
#define MOTO_IMU_SCREEN_RIGHT_AXIS 1
#endif

class MotionLeanFusion {
 public:
  explicit MotionLeanFusion(int screen_right_axis = MOTO_IMU_SCREEN_RIGHT_AXIS)
      : screen_right_axis_(screen_right_axis) {}

  void reset() noexcept {
    calibrated_ = false;
    valid_ = false;
    have_sample_ = false;
    calibration_count_ = 0;
    lean_deg_ = 0.0F;
    up_ = {0.0F, 0.0F, 1.0F};
    filtered_gyro_ = {};
    filtered_accel_ = {};
  }

  void set_speed(float speed_mps, std::uint64_t received_ms, bool usable) noexcept {
    speed_valid_ = usable && std::isfinite(speed_mps) &&
                   speed_mps >= 0.0F && speed_mps <= 100.0F;
    speed_mps_ = speed_valid_ ? speed_mps : 0.0F;
    if (speed_valid_) known_moving_ = speed_mps_ >= 0.5F;
    speed_received_ms_ = received_ms;
  }

  // Explicit rider request while parked may clear an old moving latch after
  // a phone disconnect. A fresh speed that still says moving remains a veto.
  void request_zero() noexcept {
    reset();
    known_moving_ = false;
  }

  // Call for every sensor sample, including explicit invalid/stale samples.
  // Zero is established only after 2 continuous quiet seconds while upright.
  void update(const MotionImuSample& sample) noexcept {
    if (!sample.valid || !finite(sample.accel_mps2) || !finite(sample.gyro_dps) ||
        length(sample.accel_mps2) < 0.5F || length(sample.accel_mps2) > 38.0F ||
        std::abs(sample.gyro_dps.x) >= 510.0F ||
        std::abs(sample.gyro_dps.y) >= 510.0F ||
        std::abs(sample.gyro_dps.z) >= 510.0F) {
      reset();
      return;
    }
    if (have_sample_ && sample.sample_ms <= last_sample_ms_) return;
    if (have_sample_ && sample.sample_ms - last_sample_ms_ > 160U) reset();
    const float dt = have_sample_
                         ? static_cast<float>(sample.sample_ms - last_sample_ms_) / 1000.0F
                         : 0.0F;
    last_sample_ms_ = sample.sample_ms;
    have_sample_ = true;
    const bool speed_fresh = speed_valid_ && sample.sample_ms >= speed_received_ms_ &&
                             sample.sample_ms - speed_received_ms_ <= 2000U;
    if (!calibrated_) {
      calibrate(sample, speed_fresh ? speed_mps_ < 0.5F : !known_moving_);
      return;
    }

    const MotionVector gyro = body(subtract(sample.gyro_dps, bias_));
    // Keep both filters on the same time constant so centripetal compensation
    // does not subtract a filtered turn rate from an unfiltered accelerometer.
    const float smoothing = dt / (0.020F + dt);
    filtered_gyro_ = blend(filtered_gyro_, gyro, smoothing);
    filtered_accel_ = blend(filtered_accel_, body(sample.accel_mps2), smoothing);
    const MotionVector omega = multiply(filtered_gyro_, kRadians);
    // The inertial up vector expressed in a rotating vehicle frame obeys
    // du/dt = -omega x u. Rodrigues integration preserves its unit length.
    up_ = rotate(up_, omega, -dt);

    MotionVector acceleration = filtered_accel_;
    if (speed_fresh) {
      // For velocity (v,0,0) in the vehicle frame, a_turn = omega x velocity.
      // Subtracting it avoids interpreting steady cornering force as gravity.
      acceleration = subtract(acceleration,
                               cross(omega, {speed_mps_, 0.0F, 0.0F}));
    }
    const float magnitude = length(acceleration);
    const bool quiet = length(gyro) < 1.5F &&
                       std::abs(length(sample.accel_mps2) - kGravity) < 0.6F;
    const bool moving = speed_fresh && speed_mps_ >= 0.5F;
    // Without evidence of vehicle travel, tilting the device may still supply
    // a gravity reference. Requiring near-zero gyro here expired the readout
    // after two seconds of otherwise healthy hand rotation.
    const bool stationary_reference = !known_moving_ &&
        std::abs(length(sample.accel_mps2) - kGravity) < 0.6F;
    // Reject hard longitudinal acceleration and shocks rather than pulling
    // the attitude toward a braking or vibration force. The gyro bridges gaps.
    if ((speed_fresh || stationary_reference) && magnitude > 8.0F && magnitude < 11.6F &&
        std::abs(acceleration.x - kGravity * up_.x) < 2.0F) {
      const float correction = dt / ((moving ? 0.8F : 0.35F) + dt);
      up_ = unit(blend(up_, unit(acceleration), correction));
      last_reference_ms_ = sample.sample_ms;
    }
    // Learn residual gyro bias only with fresh phone confirmation of standstill.
    if (speed_fresh && !moving && quiet) {
      bias_ = blend(bias_, sample.gyro_dps, dt / (20.0F + dt));
    }
    // Large pitch alone does not invalidate a healthy IMU. Near vertical,
    // the lateral/up projection vanishes and roll becomes ambiguous; retain
    // the last display angle there rather than turning sensor noise into a
    // large roll jump. Outside the display range, saturate the readout.
    if (std::hypot(up_.y, up_.z) >= 0.15F) {
      lean_deg_ = std::clamp(std::atan2(up_.y, up_.z) / kRadians,
                             -85.0F, 85.0F);
    }
    valid_ = sample.sample_ms - last_reference_ms_ <= 2000U &&
             std::isfinite(lean_deg_);
  }

  [[nodiscard]] bool valid() const noexcept { return valid_; }
  [[nodiscard]] bool calibrated() const noexcept { return calibrated_; }
  [[nodiscard]] float lean_deg() const noexcept { return lean_deg_; }

 private:
  static constexpr float kGravity = 9.80665F;
  static constexpr float kRadians = 0.017453292519943295F;
  int screen_right_axis_;
  bool calibrated_ = false;
  bool valid_ = false;
  bool have_sample_ = false;
  bool speed_valid_ = false;
  bool known_moving_ = false;
  float speed_mps_ = 0.0F;
  float lean_deg_ = 0.0F;
  std::uint64_t speed_received_ms_ = 0;
  std::uint64_t last_sample_ms_ = 0;
  std::uint64_t last_reference_ms_ = 0;
  std::uint64_t calibration_started_ms_ = 0;
  unsigned calibration_count_ = 0;
  MotionVector accel_sum_;
  MotionVector gyro_sum_;
  MotionVector calibration_direction_;
  MotionVector forward_;
  MotionVector left_;
  MotionVector mount_up_;
  MotionVector bias_;
  MotionVector up_{0.0F, 0.0F, 1.0F};
  MotionVector filtered_gyro_;
  MotionVector filtered_accel_;

  static bool finite(MotionVector v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
  }
  static float dot(MotionVector a, MotionVector b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
  }
  static float length(MotionVector v) noexcept { return std::sqrt(dot(v, v)); }
  static MotionVector multiply(MotionVector v, float s) noexcept {
    return {v.x * s, v.y * s, v.z * s};
  }
  static MotionVector add(MotionVector a, MotionVector b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
  }
  static MotionVector subtract(MotionVector a, MotionVector b) noexcept {
    return add(a, multiply(b, -1.0F));
  }
  static MotionVector unit(MotionVector v) noexcept {
    const float n = length(v);
    return n > 0.001F ? multiply(v, 1.0F / n) : MotionVector{};
  }
  static MotionVector cross(MotionVector a, MotionVector b) noexcept {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
             a.x * b.y - a.y * b.x};
  }
  static MotionVector blend(MotionVector a, MotionVector b, float t) noexcept {
    return add(a, multiply(subtract(b, a), t));
  }
  static MotionVector rotate(MotionVector v, MotionVector omega, float dt) noexcept {
    const float rate = length(omega);
    if (rate < 0.000001F) return v;
    const MotionVector axis = multiply(omega, 1.0F / rate);
    const float angle = rate * dt;
    return unit(add(add(multiply(v, std::cos(angle)),
                         multiply(cross(axis, v), std::sin(angle))),
                     multiply(axis, dot(axis, v) * (1.0F - std::cos(angle)))));
  }
  MotionVector body(MotionVector sensor) const noexcept {
    return {dot(sensor, forward_), dot(sensor, left_), dot(sensor, mount_up_)};
  }
  void calibrate(const MotionImuSample& sample, bool stationary_speed) noexcept {
    const float gravity = length(sample.accel_mps2);
    const MotionVector direction = unit(sample.accel_mps2);
    // Raw rates still contain the zero bias we are trying to measure.
    // This board reports about 2-3 dps at rest; a 2 dps gate continually
    // restarted calibration. Gravity direction must still remain stable.
    if (!stationary_speed || length(sample.gyro_dps) > 5.0F ||
        std::abs(gravity - kGravity) > 0.6F ||
        (calibration_count_ != 0 &&
         length(subtract(direction, calibration_direction_)) > 0.025F)) {
      calibration_count_ = 0;
      return;
    }
    if (calibration_count_ == 0) {
      calibration_started_ms_ = sample.sample_ms;
      calibration_direction_ = direction;
      accel_sum_ = {};
      gyro_sum_ = {};
    }
    accel_sum_ = add(accel_sum_, sample.accel_mps2);
    gyro_sum_ = add(gyro_sum_, sample.gyro_dps);
    ++calibration_count_;
    if (sample.sample_ms - calibration_started_ms_ < 2000U ||
        calibration_count_ < 160U) return;

    MotionVector right;
    switch (screen_right_axis_) {
      case 1: right = {1.0F, 0.0F, 0.0F}; break;
      case -1: right = {-1.0F, 0.0F, 0.0F}; break;
      case 2: right = {0.0F, 1.0F, 0.0F}; break;
      case -2: right = {0.0F, -1.0F, 0.0F}; break;
      default: calibration_count_ = 0; return;
    }
    mount_up_ = unit(accel_sum_);
    // Cannot infer a forward axis when the configured screen-right axis is
    // vertical. Leave the display invalid instead of choosing an arbitrary axis.
    if (length(cross(mount_up_, right)) < 0.5F) {
      calibration_count_ = 0;
      return;
    }
    forward_ = unit(cross(mount_up_, right));
    left_ = unit(cross(mount_up_, forward_));
    bias_ = multiply(gyro_sum_, 1.0F / calibration_count_);
    filtered_accel_ = {0.0F, 0.0F, kGravity};
    filtered_gyro_ = {};
    up_ = {0.0F, 0.0F, 1.0F};
    lean_deg_ = 0.0F;
    calibrated_ = true;
    valid_ = true;
    last_reference_ms_ = sample.sample_ms;
  }
};
