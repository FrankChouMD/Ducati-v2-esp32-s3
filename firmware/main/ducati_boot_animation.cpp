// Ducati V2 personal integration; root LICENSE.md applies to this code.
// Brand artwork has separate provenance in firmware/assets/boot/README.md.
#include "ducati_boot_animation.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "ducati_boot_assets.h"
#include "moto_nav_ui.h"

namespace {
constexpr uint32_t kFrameMs = 25;
constexpr int kOutlineCapacity = sizeof(ducati_boot_outline) / sizeof(ducati_boot_outline[0]) + 1;
constexpr float kCanvasScale = MOTO_UI_CANVAS_WIDTH / 480.0f;

struct BootState {
    lv_obj_t* root = nullptr;
    lv_timer_t* timer = nullptr;
    lv_obj_t* outline[3]{};
    lv_obj_t* shield = nullptr;
    lv_obj_t* wordmark = nullptr;
    lv_obj_t* letters[DUCATI_BOOT_GLYPH_COUNT]{};
    lv_obj_t* accent = nullptr;
    lv_point_precise_t outline_points[kOutlineCapacity]{};
    uint32_t start_ms = 0;
};
BootState boot;

float transition(uint32_t now, uint32_t start, uint32_t end) {
    if (now <= start) return 0.0f;
    if (now >= end) return 1.0f;
    const float value = static_cast<float>(now - start) / (end - start);
    return value * value * (3.0f - 2.0f * value);
}

int pixel(float value) { return static_cast<int>(std::lround(value * kCanvasScale)); }
lv_opa_t opacity(float value) {
    return static_cast<lv_opa_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
}

void screen_deleted(lv_event_t* event) {
    if (lv_event_get_target(event) != boot.root) return;
    if (boot.timer != nullptr) lv_timer_delete(boot.timer);
    boot.timer = nullptr;
    boot.root = nullptr;
}

lv_obj_t* image(lv_obj_t* parent, const lv_image_dsc_t* source) {
    lv_obj_t* object = lv_image_create(parent);
    lv_image_set_src(object, source);
    lv_image_set_pivot(object, 0, 0);
    lv_obj_remove_flag(object, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    return object;
}

void draw_outline(float progress, float settling, float alpha) {
    const auto& last = ducati_boot_outline[ducati_boot_outline_count - 1];
    const float end_distance = last.distance * progress;
    const float origin_x = 240.0f - 93.0f * settling;
    const float origin_y = 198.0f - (1538.333f * 186.0f / 1450.332f) * settling / 2.0f;
    int count = 0;
    for (int index = 0; index < ducati_boot_outline_count; ++index) {
        const auto& point = ducati_boot_outline[index];
        float x = point.x;
        float y = point.y;
        const bool at_end = point.distance > end_distance;
        if (at_end && index > 0) {
            const auto& previous = ducati_boot_outline[index - 1];
            const float ratio = (end_distance - previous.distance) / (point.distance - previous.distance);
            x = previous.x + (x - previous.x) * ratio;
            y = previous.y + (y - previous.y) * ratio;
        }
        boot.outline_points[count++] = {
            static_cast<lv_value_precise_t>(pixel(origin_x + x * settling)),
            static_cast<lv_value_precise_t>(pixel(origin_y + y * settling)),
        };
        if (at_end) break;
    }
    constexpr float strengths[] = {0.07f, 0.16f, 1.0f};
    for (int index = 0; index < 3; ++index) {
        lv_obj_set_style_line_opa(boot.outline[index], opacity(alpha * strengths[index]), 0);
        // The array is retained by LVGL. Explicitly invalidate on every edit.
        lv_line_set_points(boot.outline[index], boot.outline_points, static_cast<uint32_t>(count));
        lv_obj_invalidate(boot.outline[index]);
    }
}

void paint(uint32_t now) {
    const float exit = 1.0f - transition(now, 2060, DUCATI_BOOT_ANIMATION_DURATION_MS);
    const float reveal = transition(now, 490, 940);
    const float settling = 0.965f + 0.035f * transition(now, 100, 980);
    const float trace_alpha = transition(now, 20, 120) * (1.0f - transition(now, 630, 1030));
    draw_outline(transition(now, 70, 670), settling, trace_alpha * exit);

    const int scale = static_cast<int>(std::lround(settling * kCanvasScale * 256.0f));
    const float origin_x = 240.0f - 93.0f * settling;
    const float origin_y = 198.0f - (1538.333f * 186.0f / 1450.332f) * settling / 2.0f;
    lv_obj_set_pos(boot.shield, pixel(origin_x), pixel(origin_y));
    lv_image_set_scale(boot.shield, scale);
    lv_obj_set_style_image_opa(boot.shield, opacity(reveal * exit), 0);
    lv_obj_set_pos(boot.wordmark,
                   pixel(origin_x + ducati_boot_wordmark_x * settling),
                   pixel(origin_y + ducati_boot_wordmark_y * settling));
    lv_image_set_scale(boot.wordmark, scale);
    lv_obj_set_style_image_opa(boot.wordmark, opacity(reveal * transition(now, 650, 1060) * exit), 0);

    for (int index = 0; index < DUCATI_BOOT_GLYPH_COUNT; ++index) {
        const uint32_t start = 850 + static_cast<uint32_t>(index) * 65;
        lv_obj_set_style_image_opa(boot.letters[index], opacity(transition(now, start, start + 210) * exit), 0);
    }
    constexpr uint32_t kTextComplete = 850 + (DUCATI_BOOT_GLYPH_COUNT - 1) * 65 + 210;
    const float accent = transition(now, kTextComplete - 80, kTextComplete + 120);
    lv_obj_set_pos(boot.accent, pixel(240.0f - 24.0f * accent), pixel(376));
    lv_obj_set_width(boot.accent, std::max(1, pixel(48.0f * accent)));
    lv_obj_set_style_bg_opa(boot.accent, opacity(accent * exit), 0);
}

void animate(lv_timer_t* timer) {
    const uint32_t elapsed = lv_tick_elaps(boot.start_ms);
    paint(elapsed);
    if (elapsed >= DUCATI_BOOT_ANIMATION_DURATION_MS) {
        boot.timer = nullptr;
        lv_timer_delete(timer);
    }
}
}  // namespace

extern "C" void ducati_boot_animation_show(void) {
    static_assert(MOTO_UI_CANVAS_WIDTH == MOTO_UI_CANVAS_HEIGHT, "Boot animation expects a square canvas");
    lv_obj_t* screen = lv_screen_active();
    lv_obj_clean(screen);  // Also cancels the previous boot timer via DELETE.
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    boot.root = lv_obj_create(screen);
    lv_obj_remove_style_all(boot.root);
    lv_obj_set_size(boot.root, MOTO_UI_CANVAS_WIDTH, MOTO_UI_CANVAS_HEIGHT);
    lv_obj_remove_flag(boot.root, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_add_event_cb(boot.root, screen_deleted, LV_EVENT_DELETE, nullptr);

    constexpr int widths[] = {10, 6, 2};
    for (int index = 0; index < 3; ++index) {
        boot.outline[index] = lv_line_create(boot.root);
        lv_obj_set_pos(boot.outline[index], 0, 0);
        lv_obj_set_style_line_color(boot.outline[index], lv_color_hex(0xE32431), 0);
        lv_obj_set_style_line_width(boot.outline[index], std::max(1, pixel(widths[index])), 0);
        lv_obj_set_style_line_rounded(boot.outline[index], true, 0);
    }
    boot.shield = image(boot.root, &ducati_boot_shield_base);
    boot.wordmark = image(boot.root, &ducati_boot_wordmark);
    for (int index = 0; index < DUCATI_BOOT_GLYPH_COUNT; ++index) {
        const auto& glyph = ducati_boot_glyphs[index];
        boot.letters[index] = image(boot.root, glyph.image);
        lv_obj_set_pos(boot.letters[index], pixel(glyph.x), pixel(glyph.y));
        lv_image_set_scale(boot.letters[index], static_cast<uint32_t>(std::lround(kCanvasScale * 256.0f)));
    }
    boot.accent = lv_obj_create(boot.root);
    lv_obj_remove_style_all(boot.accent);
    lv_obj_set_height(boot.accent, std::max(1, pixel(3)));
    lv_obj_set_style_radius(boot.accent, pixel(2), 0);
    lv_obj_set_style_bg_color(boot.accent, lv_color_hex(0xE32431), 0);
    boot.start_ms = lv_tick_get();
    paint(0);
    boot.timer = lv_timer_create(animate, kFrameMs, nullptr);
}
