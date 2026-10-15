// Render the firmware's actual LVGL animation and verify its visible phases.
#include "ducati_boot_animation.h"
#include "moto_nav_ui.h"
#include "lvgl.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
constexpr int kWidth = MOTO_UI_CANVAS_WIDTH;
constexpr int kHeight = MOTO_UI_CANVAS_HEIGHT;
std::vector<std::uint16_t> frame(kWidth * kHeight);
int failures = 0;
const char* output_directory = nullptr;

#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << __LINE__ << " CHECK failed: " #condition << '\n'; ++failures; \
} } while (false)

void flush(lv_display_t* display, const lv_area_t* area, std::uint8_t* pixels) {
    const auto* source = reinterpret_cast<const std::uint16_t*>(pixels);
    const int width = area->x2 - area->x1 + 1;
    for (int y = area->y1; y <= area->y2; ++y) {
        std::memcpy(frame.data() + y * kWidth + area->x1,
                    source + (y - area->y1) * width, width * sizeof(std::uint16_t));
    }
    lv_display_flush_ready(display);
}

void advance(int milliseconds) {
    for (int elapsed = 0; elapsed < milliseconds; elapsed += 5) {
        lv_tick_inc(5);
        lv_timer_handler();
    }
    lv_refr_now(lv_display_get_default());
}

int pixels_in(int x1, int y1, int x2, int y2) {
    const auto scaled = [](int value) { return (value * kWidth + 240) / 480; };
    int count = 0;
    for (int y = scaled(y1); y < scaled(y2); ++y) {
        for (int x = scaled(x1); x < scaled(x2); ++x) {
            if (frame[y * kWidth + x] != 0) ++count;
        }
    }
    return count;
}

bool black() {
    return std::all_of(frame.begin(), frame.end(), [](std::uint16_t color) { return color == 0; });
}

void save_frame(const char* name) {
    if (output_directory == nullptr) return;
    std::ofstream output(std::string(output_directory) + "/" + name + ".ppm", std::ios::binary);
    output << "P6\n" << kWidth << ' ' << kHeight << "\n255\n";
    for (std::uint16_t color : frame) {
        const char bytes[] = {
            static_cast<char>((((color >> 11) & 31) * 255 + 15) / 31),
            static_cast<char>((((color >> 5) & 63) * 255 + 31) / 63),
            static_cast<char>(((color & 31) * 255 + 15) / 31),
        };
        output.write(bytes, 3);
    }
    CHECK(output.good());
}

void start() {
    ducati_boot_animation_show();
    lv_refr_now(lv_display_get_default());
}
}  // namespace

int main(int argc, char** argv) {
    if (argc > 1) output_directory = argv[1];
    lv_init();
    std::vector<std::uint32_t> extra_pool(2 * 1024 * 1024 / 4);
    CHECK(lv_mem_add_pool(extra_pool.data(), extra_pool.size() * sizeof(std::uint32_t)) != nullptr);
    static std::vector<std::uint16_t> draw_buffer(kWidth * 40);
    lv_display_t* display = lv_display_create(kWidth, kHeight);
    lv_display_set_flush_cb(display, flush);
    lv_display_set_buffers(display, draw_buffer.data(), nullptr,
                           draw_buffer.size() * sizeof(std::uint16_t), LV_DISPLAY_RENDER_MODE_PARTIAL);

    start();
    CHECK(black());  // The physical panel is revealed only after this frame.
    advance(425);
    CHECK(pixels_in(130, 80, 350, 310) > 0);  // Red outline, before the badge.
    CHECK(pixels_in(100, 320, 380, 365) == 0);
    save_frame("outline-425ms");
    advance(725);  // 1150ms: the first letters lead the still-hidden suffix.
    CHECK(pixels_in(120, 325, 215, 360) > 0);
    CHECK(pixels_in(295, 325, 375, 360) == 0);
    save_frame("letters-1150ms");
    advance(750);  // 1900ms: the complete approved logo and model name.
    CHECK(pixels_in(295, 325, 375, 360) > 0);
    CHECK(pixels_in(215, 375, 265, 382) > 0);
    const auto complete = frame;
    save_frame("complete-1900ms");
    advance(DUCATI_BOOT_ANIMATION_DURATION_MS - 1900 + 50);
    CHECK(black());

    // Replacing a partly animated tree cancels its timer and restarts black.
    start();
    advance(1050);
    start();
    CHECK(black());
    advance(1900);
    CHECK(frame == complete);

    // The real navigation constructor destroys the intro and owns the screen.
    // Continue well past the original timer deadline to expose stale widgets.
    moto_nav_ui_create();
    advance(2800);
    CHECK(!black());

    lv_display_delete(display);
    lv_deinit();
    if (failures) return EXIT_FAILURE;
    std::cout << "Boot intro: black start/end, sequential model letters, repeat and navigation handoff passed\n";
    return EXIT_SUCCESS;
}
