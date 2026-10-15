#pragma once

#include <cstdint>

#include "esp_err.h"
#include "motion_imu_sample.hpp"

class MotionHeadingSensor {
 public:
  using SampleCallback = void (*)(const MotionImuSample& sample, void* context);

  MotionHeadingSensor() = default;
  MotionHeadingSensor(const MotionHeadingSensor&) = delete;
  MotionHeadingSensor& operator=(const MotionHeadingSensor&) = delete;

  // Starts one low-priority 125 Hz sensor task. Failure is non-fatal: phone
  // course navigation continues to work without the local latency filler.
  esp_err_t start(SampleCallback callback, void* context);

 private:
  static void task_entry(void* context);
  void run();

  SampleCallback callback_ = nullptr;
  void* callback_context_ = nullptr;
  bool started_ = false;
};
