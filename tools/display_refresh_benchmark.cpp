// Optional on-device benchmark, enabled by -DDUCATI_DISPLAY_BENCHMARK=ON.
// Runs after BLE starts and before live IMU samples; never in normal firmware.
#include "board_port.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace {
std::uint64_t pixels = 0;
void count_pixels(lv_event_t* event) {
  const auto* area = static_cast<const lv_area_t*>(lv_event_get_param(event));
  pixels += lv_area_get_size(area);
}
}

extern "C" void ducati_display_benchmark() {
  constexpr int frames = 40;
  static moto_ui_state_t state;
  auto* display = board_port_get_display();
  board_port_lock(UINT32_MAX);
  lv_display_add_event_cb(display, count_pixels, LV_EVENT_FLUSH_START, nullptr);
  moto_nav_ui_set_demo_active(1);
  moto_nav_ui_set_reduce_motion(1);
  // Keep the synthetic page stable even if a paired phone reconnects and
  // queues navigation updates. BLE stays active; only UI application waits.
  for(const auto page : {MOTO_UI_PAGE_NAVIGATION, MOTO_UI_PAGE_SPEED}) {
    for(bool full : {true, false}) {
      state = {};
      state.page = page;
      state.mode = MOTO_UI_NAVIGATING;
      state.lean_valid = 1;
      state.speed_kph = 58;
      state.speed_limit_kph = 50;
      state.distance_to_maneuver_m = 180;
      state.has_destination = 1;
      state.route_point_count = 3;
      state.route_points[0] = {MOTO_UI_NAV_VEHICLE_X, MOTO_UI_NAV_VEHICLE_Y};
      state.route_points[1] = {240, 130};
      state.route_points[2] = {330, 60};
      moto_nav_ui_set_state(&state);
      lv_refr_now(display);
      std::array<std::int64_t, frames> durations{};
      std::uint64_t transferred = 0;
      for(int i = 0; i < frames; ++i) {
        const auto before_pixels = pixels;
        const auto start = esp_timer_get_time();
        state.lean_deg = 35.0F * std::sin(i * 0.23F);
        moto_nav_ui_set_lean_state(&state);
        if(full) lv_obj_invalidate(lv_screen_active());
        lv_refr_now(display);
        durations[i] = esp_timer_get_time() - start;
        transferred += pixels - before_pixels;
        vTaskDelay(1);
      }
      std::int64_t total = 0;
      for(const auto duration : durations) total += duration;
      std::sort(durations.begin(), durations.end());
      ESP_LOGI("display_bench", "page=%d mode=%s avg_us=%lld p95_us=%lld pixels_per_frame=%llu internal_free=%u",
               static_cast<int>(page), full ? "full" : "lean", total / frames,
               durations[frames * 95 / 100], transferred / frames,
               static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));
    }
  }
  lv_display_remove_event_cb_with_user_data(display, count_pixels, nullptr);
  moto_nav_ui_set_reduce_motion(0);
  moto_nav_ui_set_demo_active(0);
  state = {};
  moto_nav_ui_set_state(&state);
  board_port_unlock();
}
