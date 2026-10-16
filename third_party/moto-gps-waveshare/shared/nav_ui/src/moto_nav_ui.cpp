#include "moto_nav_ui.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>

#include "lvgl.h"
#include "ducati_rear_e.h"
#if MOTO_UI_SQUARE_SCREEN
#include "ducati_top.h"
#endif
#include "fonts/ducati_instrument_fonts.h"

// Ducati V2 integration: the generated font is compiled as C. Keep the
// declaration's linkage explicit for MSVC as well as the ESP32 toolchain.
extern "C" {
LV_FONT_DECLARE(moto_font_nav_16);
}

namespace {

// Ducati-inspired instruments: black, neutral white and racing-red accents.
// Status colours retain their meaning; green is not a simulated gear signal.
constexpr lv_color_t kBlack = LV_COLOR_MAKE(0x00, 0x00, 0x00);
constexpr lv_color_t kWhite = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF);
constexpr lv_color_t kGraphite = LV_COLOR_MAKE(0x30, 0x32, 0x34);
// Real surrounding roads need to remain legible on the AMOLED's true-black
// background.  LV_OPA_70 is 70/255 (not 70 percent), which made the previous
// road layer effectively disappear on the physical display.
constexpr lv_color_t kRoadGray = LV_COLOR_MAKE(0x42, 0x47, 0x4B);
constexpr lv_color_t kRoadMajor = LV_COLOR_MAKE(0x62, 0x69, 0x6D);
constexpr lv_color_t kRoadMinor = LV_COLOR_MAKE(0x2B, 0x30, 0x33);
constexpr lv_color_t kBuildingGray = LV_COLOR_MAKE(0x24, 0x29, 0x2C);
constexpr lv_color_t kBuildingLandmark = LV_COLOR_MAKE(0x38, 0x40, 0x44);
constexpr lv_color_t kQuiet = LV_COLOR_MAKE(0x9B, 0x9B, 0x9B);
constexpr lv_color_t kSoft = LV_COLOR_MAKE(0xCF, 0xCF, 0xCF);
constexpr lv_color_t kAmber = LV_COLOR_MAKE(0xFF, 0xC4, 0x00);
constexpr lv_color_t kRed = LV_COLOR_MAKE(0xE8, 0x00, 0x16);
constexpr lv_color_t kGreen = LV_COLOR_MAKE(0xA6, 0xD8, 0x00);
constexpr double kPi = 3.14159265358979323846;
constexpr int kCompassTickCount = 24;
constexpr int kSpeedTickCount = 17;  // 0..160 km/h, in steps of 10.
constexpr int kDesignWidth = 360;
constexpr std::uint32_t kPageDotsVisibleMs = 5'000;
// Keep LVGL, the UI interpolation timer and IMU presentation on the same
// 40 Hz cadence. The previous 40/33 ms mismatch periodically produced a
// 66 ms visual gap even when both tasks were otherwise keeping up.
constexpr std::uint32_t kRouteMotionFrameMs = 25;
static_assert(LV_DEF_REFR_PERIOD == kRouteMotionFrameMs,
              "LVGL refresh and map motion must use the same cadence");
constexpr std::uint32_t kConnectionSuccessHoldMs = 920;

enum class LifecycleVisual : std::uint8_t {
    Hidden = 0,
    PhoneOffline,
    PhoneConnecting,
    Connected,
    Ready,
    Planning,
    Locating,
    SyncingRoute,
};

constexpr int px(int value) {
    return value >= 0
               ? (value * MOTO_UI_CANVAS_WIDTH + kDesignWidth / 2) /
                     kDesignWidth
               : -((-value * MOTO_UI_CANVAS_WIDTH + kDesignWidth / 2) /
                   kDesignWidth);
}

constexpr double px(double value) {
    return value * static_cast<double>(MOTO_UI_CANVAS_WIDTH) /
           static_cast<double>(kDesignWidth);
}

struct MapPolyline {
    const lv_point_precise_t *points = nullptr;
    std::uint16_t count = 0;
};

struct LeanGraphic {
    lv_obj_t *object = nullptr;
    lv_obj_t *reference = nullptr;
    float angle_deg = 0.0F;
    bool valid = false;
};

struct Ui {
    lv_obj_t *screen = nullptr;
    lv_obj_t *pages[MOTO_UI_PAGE_COUNT]{};
    lv_obj_t *page_dots[MOTO_UI_PAGE_COUNT]{};
    lv_timer_t *page_dots_timer = nullptr;
    moto_ui_page_t page = MOTO_UI_PAGE_NAVIGATION;
    bool page_dots_visible = true;

    lv_obj_t *nav_map = nullptr;
    lv_obj_t *nav_buildings[MOTO_UI_BUILDING_FOOTPRINT_CAPACITY]{};
    MapPolyline nav_building_lines[MOTO_UI_BUILDING_FOOTPRINT_CAPACITY]{};
    lv_point_precise_t nav_building_points[
        MOTO_UI_BUILDING_POINT_CAPACITY +
        MOTO_UI_BUILDING_FOOTPRINT_CAPACITY]{};
    float nav_building_x[MOTO_UI_BUILDING_POINT_CAPACITY]{};
    float nav_building_y[MOTO_UI_BUILDING_POINT_CAPACITY]{};
    float nav_building_target_x[MOTO_UI_BUILDING_POINT_CAPACITY]{};
    float nav_building_target_y[MOTO_UI_BUILDING_POINT_CAPACITY]{};
    moto_ui_building_span_t
        nav_building_spans[MOTO_UI_BUILDING_FOOTPRINT_CAPACITY]{};
    std::uint8_t nav_building_point_count = 0;
    std::uint8_t nav_building_footprint_count = 0;
    std::uint32_t nav_building_scene_revision = 0;
    lv_obj_t *nav_roads[MOTO_UI_ROAD_POLYLINE_CAPACITY]{};
    MapPolyline nav_road_lines[MOTO_UI_ROAD_POLYLINE_CAPACITY]{};
    lv_point_precise_t nav_road_points[MOTO_UI_ROAD_POINT_CAPACITY]{};
    float nav_road_x[MOTO_UI_ROAD_POINT_CAPACITY]{};
    float nav_road_y[MOTO_UI_ROAD_POINT_CAPACITY]{};
    float nav_road_target_x[MOTO_UI_ROAD_POINT_CAPACITY]{};
    float nav_road_target_y[MOTO_UI_ROAD_POINT_CAPACITY]{};
    moto_ui_polyline_span_t
        nav_road_spans[MOTO_UI_ROAD_POLYLINE_CAPACITY]{};
    std::uint8_t nav_road_point_count = 0;
    std::uint8_t nav_road_polyline_count = 0;
    std::uint32_t nav_road_scene_revision = 0;
    lv_obj_t *nav_route_shadow = nullptr;
    lv_obj_t *nav_route = nullptr;
    MapPolyline nav_route_shadow_line{};
    MapPolyline nav_route_line{};
    lv_point_precise_t nav_route_points[MOTO_UI_ROUTE_POINT_CAPACITY]{};
    float nav_route_x[MOTO_UI_ROUTE_POINT_CAPACITY]{};
    float nav_route_y[MOTO_UI_ROUTE_POINT_CAPACITY]{};
    float nav_route_target_x[MOTO_UI_ROUTE_POINT_CAPACITY]{};
    float nav_route_target_y[MOTO_UI_ROUTE_POINT_CAPACITY]{};
    std::uint8_t nav_route_point_count = 0;
    std::uint8_t nav_route_target_count = 0;
    std::uint32_t nav_route_identity = 0;
    std::uint32_t nav_route_generation = 0;
    lv_timer_t *nav_route_motion_timer = nullptr;
    lv_obj_t *nav_marker = nullptr;
    lv_obj_t *nav_status = nullptr;
    lv_obj_t *nav_distance = nullptr;
    lv_obj_t *nav_unit = nullptr;
    lv_obj_t *nav_maneuver = nullptr;
    moto_maneuver_t nav_maneuver_type = MOTO_MANEUVER_STRAIGHT;
    lv_obj_t *nav_limit = nullptr;
    lv_obj_t *nav_limit_value = nullptr;
    lv_obj_t *nav_progress = nullptr;
    lv_obj_t *nav_speed_gauge = nullptr;
    lv_obj_t *nav_speed_value = nullptr;
    std::uint16_t nav_speed_kph = 0;
    bool nav_speed_available = true;
    lv_obj_t *nav_lean_value = nullptr;
    LeanGraphic nav_lean_graphic;

    // A full-screen empty-state surface replaces the old tiny
    // WAITING FOR PHONE / WAITING FOR ROUTE caption. It is kept inside the
    // navigation page so the speed and compass tools remain independently
    // usable while the phone is disconnected.
    lv_obj_t *nav_lifecycle = nullptr;
    lv_obj_t *nav_lifecycle_symbol = nullptr;
    lv_obj_t *nav_lifecycle_title = nullptr;
    lv_obj_t *nav_lifecycle_subtitle = nullptr;
    lv_obj_t *nav_lifecycle_kicker = nullptr;
    lv_timer_t *nav_lifecycle_timer = nullptr;
    lv_timer_t *nav_success_timer = nullptr;
    moto_ui_phone_connection_t phone_connection = MOTO_UI_PHONE_OFFLINE;
    LifecycleVisual lifecycle_visual = LifecycleVisual::Hidden;
    LifecycleVisual lifecycle_target = LifecycleVisual::Hidden;
    std::uint16_t lifecycle_phase_deg = 0;
    bool lifecycle_success_active = false;
    bool navigation_has_route_display = false;
    bool navigation_has_destination = false;
    bool navigation_has_usable_fix = false;
    bool navigation_route_request_in_flight = false;

    lv_obj_t *speed_arc = nullptr;
    lv_obj_t *speed_value = nullptr;
    lv_obj_t *speed_lean_value = nullptr;
    LeanGraphic speed_lean_graphic;
    lv_obj_t *speed_ticks[kSpeedTickCount]{};
    lv_point_precise_t speed_tick_points[kSpeedTickCount][2]{};

    lv_obj_t *compass_heading = nullptr;
    lv_obj_t *compass_cardinal = nullptr;
    lv_obj_t *compass_speed = nullptr;
    lv_obj_t *compass_ticks[kCompassTickCount]{};
    lv_point_precise_t compass_tick_points[kCompassTickCount][2]{};
    lv_obj_t *compass_letters[4]{};

    lv_obj_t *music_source = nullptr;
    lv_obj_t *music_title = nullptr;
    lv_obj_t *music_artist = nullptr;
    lv_obj_t *music_disc = nullptr;
    lv_obj_t *music_buttons[4]{};
    lv_obj_t *music_button_labels[4]{};
    moto_music_state_t music{};
    char music_source_text[32]{};
    char music_title_text[64]{};
    char music_artist_text[48]{};

    moto_music_command_callback_t music_callback = nullptr;
    void *music_callback_context = nullptr;
    moto_page_change_callback_t page_callback = nullptr;
    void *page_callback_context = nullptr;
    moto_demo_change_callback_t demo_callback = nullptr;
    void *demo_callback_context = nullptr;
    moto_lean_calibration_callback_t lean_calibration_callback = nullptr;
    void *lean_calibration_context = nullptr;
    bool music_page_enabled = true;
    bool demo_active = false;
    bool reduce_motion = false;
} ui;

void reset_ui_state() {
    static_assert(std::is_trivially_copyable_v<Ui>,
                  "Ui must remain safe to reset without a stack temporary");

    // `ui = {}` materializes the complete aggregate as a temporary.  The
    // offline map buffers make Ui about 15 KiB, so that temporary can consume
    // nearly the whole ESP-IDF main-task stack before LVGL is called.  Clear
    // the retained instance in place and restore the two non-zero defaults.
    std::memset(&ui, 0, sizeof(ui));
    ui.page = MOTO_UI_PAGE_NAVIGATION;
    ui.page_dots_visible = true;
    ui.nav_maneuver_type = MOTO_MANEUVER_STRAIGHT;
    ui.phone_connection = MOTO_UI_PHONE_OFFLINE;
    ui.lifecycle_visual = LifecycleVisual::Hidden;
    ui.lifecycle_target = LifecycleVisual::Hidden;
    ui.music_page_enabled = true;
    // Force the first geometry payload to bind every mutable LVGL line even
    // when its protocol revision happens to start at zero.
    ui.nav_route_generation = ~std::uint32_t{0};
    ui.nav_route_identity = ~std::uint32_t{0};
    ui.nav_road_scene_revision = ~std::uint32_t{0};
    ui.nav_building_scene_revision = ~std::uint32_t{0};
}

lv_obj_t *make_layer(lv_obj_t *parent) {
    lv_obj_t *layer = lv_obj_create(parent);
    lv_obj_remove_style_all(layer);
    lv_obj_set_size(layer, MOTO_UI_CANVAS_WIDTH, MOTO_UI_CANVAS_HEIGHT);
    lv_obj_set_pos(layer, 0, 0);
    lv_obj_remove_flag(layer, LV_OBJ_FLAG_SCROLLABLE);
    return layer;
}

lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font,
                     lv_color_t color, const char *text) {
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_label_set_text(label, text);
    return label;
}

void copy_text(char *destination, std::size_t capacity, const char *source,
               const char *fallback) {
    if(capacity == 0) return;
    const char *value = source != nullptr && source[0] != '\0' ? source : fallback;
    std::snprintf(destination, capacity, "%s", value != nullptr ? value : "");
}

lv_color_t traffic_color(moto_traffic_t traffic) {
    switch(traffic) {
        case MOTO_TRAFFIC_CLEAR: return kWhite;
        case MOTO_TRAFFIC_SLOW: return kAmber;
        case MOTO_TRAFFIC_CONGESTED:
        case MOTO_TRAFFIC_SEVERE: return kRed;
        default: return kQuiet;
    }
}

const char *cardinal_name(std::uint16_t heading) {
    static const char *names[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    return names[((heading + 22U) / 45U) % 8U];
}

void draw_vehicle_marker(lv_event_t *event) {
    lv_obj_t *object = lv_event_get_target_obj(event);
    lv_area_t area;
    lv_obj_get_coords(object, &area);
    lv_layer_t *layer = lv_event_get_layer(event);

    lv_draw_triangle_dsc_t triangle;
    lv_draw_triangle_dsc_init(&triangle);
    triangle.color = kBlack;
    triangle.opa = LV_OPA_COVER;
    triangle.p[0] = {static_cast<lv_value_precise_t>(area.x1 + px(17)),
                     static_cast<lv_value_precise_t>(area.y1)};
    triangle.p[1] = {static_cast<lv_value_precise_t>(area.x1),
                     static_cast<lv_value_precise_t>(area.y1 + px(36))};
    triangle.p[2] = {static_cast<lv_value_precise_t>(area.x1 + px(34)),
                     static_cast<lv_value_precise_t>(area.y1 + px(36))};
    lv_draw_triangle(layer, &triangle);

    triangle.color = kWhite;
    triangle.p[0] = {static_cast<lv_value_precise_t>(area.x1 + px(17)),
                     static_cast<lv_value_precise_t>(area.y1 + px(5))};
    triangle.p[1] = {static_cast<lv_value_precise_t>(area.x1 + px(6)),
                     static_cast<lv_value_precise_t>(area.y1 + px(30))};
    triangle.p[2] = {static_cast<lv_value_precise_t>(area.x1 + px(28)),
                     static_cast<lv_value_precise_t>(area.y1 + px(30))};
    lv_draw_triangle(layer, &triangle);
}

void update_page_dots() {
    for(int i = 0; i < MOTO_UI_PAGE_COUNT; ++i) {
        const bool active = i == static_cast<int>(ui.page);
        lv_obj_set_size(ui.page_dots[i], px(active ? 16 : 5), px(5));
        lv_obj_set_style_radius(ui.page_dots[i], px(3), 0);
        lv_obj_set_style_bg_color(ui.page_dots[i], active ? kRed : kGraphite, 0);
        lv_obj_set_x(ui.page_dots[i], px(145 + i * 22 - (active ? 5 : 0)));
        lv_obj_set_y(ui.page_dots[i], px(MOTO_UI_SQUARE_SCREEN ?
                          (ui.page == MOTO_UI_PAGE_NAVIGATION ? 355 : 348) : 337));
        const bool available = i != MOTO_UI_PAGE_MUSIC || ui.music_page_enabled;
        if(ui.page_dots_visible && available) {
            lv_obj_remove_flag(ui.page_dots[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(ui.page_dots[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void hide_page_dots(lv_timer_t *) {
    ui.page_dots_visible = false;
    update_page_dots();
    if(ui.page_dots_timer != nullptr) lv_timer_pause(ui.page_dots_timer);
}

void reveal_page_dots() {
    if(ui.screen == nullptr || ui.page_dots[0] == nullptr) return;
    ui.page_dots_visible = true;
    update_page_dots();
    if(ui.page_dots_timer != nullptr) {
        lv_timer_set_period(ui.page_dots_timer, kPageDotsVisibleMs);
        lv_timer_reset(ui.page_dots_timer);
        lv_timer_resume(ui.page_dots_timer);
    }
}

void interaction_event(lv_event_t *) {
    reveal_page_dots();
}

void install_interaction_wake(lv_obj_t *object) {
    if(object == nullptr) return;
    lv_obj_add_event_cb(object, interaction_event, LV_EVENT_PRESSED, nullptr);
    const std::uint32_t child_count = lv_obj_get_child_count(object);
    for(std::uint32_t index = 0; index < child_count; ++index) {
        install_interaction_wake(lv_obj_get_child(object,
                                                  static_cast<int32_t>(index)));
    }
}

void show_page(moto_ui_page_t page, bool reveal_on_same_page = false) {
    if(page < MOTO_UI_PAGE_NAVIGATION || page >= MOTO_UI_PAGE_COUNT) return;
    if(page == MOTO_UI_PAGE_MUSIC && !ui.music_page_enabled) {
        page = MOTO_UI_PAGE_NAVIGATION;
    }
    const bool changed = page != ui.page;
    ui.page = page;
    for(int i = 0; i < MOTO_UI_PAGE_COUNT; ++i) {
        if(i == static_cast<int>(page)) {
            lv_obj_remove_flag(ui.pages[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(ui.pages[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    update_page_dots();
    // Navigation state arrives continuously. Only a real page transition or
    // user action may restart the five-second affordance timer.
    if(changed || reveal_on_same_page) reveal_page_dots();
}

void gesture_event(lv_event_t *) {
    lv_indev_t *indev = lv_indev_active();
    if(indev == nullptr) return;
    const lv_dir_t direction = lv_indev_get_gesture_dir(indev);
    int next = static_cast<int>(ui.page);
    if(direction == LV_DIR_LEFT) {
        next = (next + 1) % MOTO_UI_PAGE_COUNT;
    } else if(direction == LV_DIR_RIGHT) {
        next = (next + MOTO_UI_PAGE_COUNT - 1) % MOTO_UI_PAGE_COUNT;
    } else {
        return;
    }
    if(!ui.music_page_enabled && next == MOTO_UI_PAGE_MUSIC) {
        next = direction == LV_DIR_LEFT ? MOTO_UI_PAGE_NAVIGATION
                                        : MOTO_UI_PAGE_COMPASS;
    }
    const auto requested = static_cast<moto_ui_page_t>(next);
    if(ui.page_callback != nullptr) {
        ui.page_callback(requested, ui.page_callback_context);
    } else {
        show_page(requested, true);
    }
    reveal_page_dots();
    lv_indev_wait_release(indev);
}

void draw_map_line(lv_layer_t *layer, const lv_draw_line_dsc_t &line) {
    const lv_area_t original_clip = layer->_clip_area;
    lv_area_t visible{
        std::max(original_clip.x1, static_cast<int32_t>(
            std::min(line.p1.x, line.p2.x) - line.width)),
        std::max(original_clip.y1, static_cast<int32_t>(
            std::min(line.p1.y, line.p2.y) - line.width)),
        std::min(original_clip.x2, static_cast<int32_t>(
            std::max(line.p1.x, line.p2.x) + line.width)),
        std::min(original_clip.y2, static_cast<int32_t>(
            std::max(line.p1.y, line.p2.y) + line.width)),
    };
    if(visible.x1 > visible.x2 || visible.y1 > visible.y2) return;
    const float dx = static_cast<float>(line.p2.x - line.p1.x);
    const float dy = static_cast<float>(line.p2.y - line.p1.y);
    constexpr int32_t band_height = 32;
    if(std::abs(dy) <= band_height || std::abs(dx) <= 64.0F ||
       visible.y2 - visible.y1 < band_height) {
        lv_draw_line(layer, &line);
        return;
    }

    // LVGL masks every pixel in a diagonal's bounding rectangle, including
    // the empty space beside a long thin street. Narrow that rectangle per
    // non-overlapping band. Keep the original endpoints so antialiasing and
    // round caps remain identical, including where adjacent bands meet.
    const float slope = dx / dy;
    const float padding = static_cast<float>(line.width) + 2.0F;
    const float minimum_y = static_cast<float>(std::min(line.p1.y, line.p2.y));
    const float maximum_y = static_cast<float>(std::max(line.p1.y, line.p2.y));
    for(int32_t y = visible.y1; y <= visible.y2; y += band_height) {
        lv_area_t band = visible;
        band.y1 = y;
        band.y2 = std::min(y + band_height - 1, visible.y2);
        const float low = std::clamp(static_cast<float>(band.y1) - padding,
                                     minimum_y, maximum_y);
        const float high = std::clamp(static_cast<float>(band.y2) + padding,
                                      minimum_y, maximum_y);
        const float a = line.p1.x + slope * (low - line.p1.y);
        const float b = line.p1.x + slope * (high - line.p1.y);
        band.x1 = std::max(band.x1, static_cast<int32_t>(
            std::floor(std::min(a, b) - padding)));
        band.x2 = std::min(band.x2, static_cast<int32_t>(
            std::ceil(std::max(a, b) + padding)));
        if(band.x1 > band.x2) continue;
        // lv_draw_line copies this clip into its task before dispatching.
        layer->_clip_area = band;
        lv_draw_line(layer, &line);
    }
    layer->_clip_area = original_clip;
}

void draw_map_polyline(lv_event_t *event) {
    lv_obj_t *object = lv_event_get_target_obj(event);
    const auto code = lv_event_get_code(event);
    if(code == LV_EVENT_REFR_EXT_DRAW_SIZE) {
        auto *size = static_cast<int32_t *>(lv_event_get_param(event));
        *size = std::max(*size, lv_obj_get_style_line_width(object, LV_PART_MAIN));
        return;
    }
    if(code != LV_EVENT_DRAW_MAIN) return;
    const auto *polyline = static_cast<const MapPolyline *>(
        lv_event_get_user_data(event));
    if(polyline->points == nullptr || polyline->count < 2) return;
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_area_t object_area;
    lv_obj_get_coords(object, &object_area);
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.base.layer = layer;
    lv_obj_init_draw_line_dsc(object, LV_PART_MAIN, &line);
    for(std::uint16_t index = 1; index < polyline->count; ++index) {
        line.p1 = polyline->points[index - 1];
        line.p2 = polyline->points[index];
        line.p1.x += object_area.x1;
        line.p2.x += object_area.x1;
        line.p1.y += object_area.y1;
        line.p2.y += object_area.y1;
        draw_map_line(layer, line);
        line.round_start = 0;
    }
}

lv_obj_t *create_map_polyline(lv_obj_t *parent, MapPolyline &polyline) {
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_user_data(object, &polyline);
    lv_obj_add_event_cb(object, draw_map_polyline, LV_EVENT_ALL, &polyline);
    return object;
}

void set_map_polyline_points(lv_obj_t *object, const lv_point_precise_t *points,
                            std::uint16_t count) {
    auto *polyline = static_cast<MapPolyline *>(lv_obj_get_user_data(object));
    polyline->points = points;
    polyline->count = count;
    lv_obj_invalidate(object);
}

bool apply_route_geometry_frame(bool rebind_lines = true) {
    if(ui.nav_route_point_count < 2) return false;
    bool pixels_changed = false;
    for(std::uint8_t i = 0; i < ui.nav_route_point_count; ++i) {
        const auto x = static_cast<lv_value_precise_t>(
            std::lround(ui.nav_route_x[i]));
        const auto y = static_cast<lv_value_precise_t>(
            std::lround(ui.nav_route_y[i]));
        pixels_changed = pixels_changed || ui.nav_route_points[i].x != x ||
                         ui.nav_route_points[i].y != y;
        ui.nav_route_points[i] = {x, y};
    }
    if(rebind_lines) {
        set_map_polyline_points(ui.nav_route_shadow, ui.nav_route_points,
                                   ui.nav_route_point_count);
        set_map_polyline_points(ui.nav_route, ui.nav_route_points,
                                   ui.nav_route_point_count);
    }
    return pixels_changed || rebind_lines;
}

bool apply_road_geometry_frame(bool rebind_lines = true) {
    if(ui.nav_road_point_count < 2 || ui.nav_road_polyline_count == 0) {
        return false;
    }
    bool pixels_changed = false;
    for(std::uint8_t i = 0; i < ui.nav_road_point_count; ++i) {
        const auto x = static_cast<lv_value_precise_t>(
            std::lround(ui.nav_road_x[i]));
        const auto y = static_cast<lv_value_precise_t>(
            std::lround(ui.nav_road_y[i]));
        pixels_changed = pixels_changed || ui.nav_road_points[i].x != x ||
                         ui.nav_road_points[i].y != y;
        ui.nav_road_points[i] = {x, y};
    }
    if(rebind_lines) {
        for(std::uint8_t i = 0; i < ui.nav_road_polyline_count; ++i) {
            const moto_ui_polyline_span_t span = ui.nav_road_spans[i];
            set_map_polyline_points(
                ui.nav_roads[i],
                &ui.nav_road_points[span.first_point_index],
                span.point_count);
        }
    }
    return pixels_changed || rebind_lines;
}

bool apply_building_geometry_frame(bool rebind_lines = true) {
    if(ui.nav_building_point_count < 3 ||
       ui.nav_building_footprint_count == 0) return false;
    bool pixels_changed = false;
    std::size_t packed_index = 0;
    for(std::uint8_t footprint_index = 0;
        footprint_index < ui.nav_building_footprint_count;
        ++footprint_index) {
        const moto_ui_building_span_t span =
            ui.nav_building_spans[footprint_index];
        for(std::uint8_t point_index = 0; point_index < span.point_count;
            ++point_index) {
            const std::size_t source = span.first_point_index + point_index;
            const auto x = static_cast<lv_value_precise_t>(
                std::lround(ui.nav_building_x[source]));
            const auto y = static_cast<lv_value_precise_t>(
                std::lround(ui.nav_building_y[source]));
            pixels_changed = pixels_changed ||
                ui.nav_building_points[packed_index].x != x ||
                ui.nav_building_points[packed_index].y != y;
            ui.nav_building_points[packed_index++] = {x, y};
        }
        // BLE omits the duplicate closing point; LVGL needs it explicitly.
        const auto closing =
            ui.nav_building_points[packed_index - span.point_count];
        pixels_changed = pixels_changed ||
            ui.nav_building_points[packed_index].x != closing.x ||
            ui.nav_building_points[packed_index].y != closing.y;
        ui.nav_building_points[packed_index] = closing;
        if(rebind_lines) {
            set_map_polyline_points(
                ui.nav_buildings[footprint_index],
                &ui.nav_building_points[packed_index - span.point_count],
                span.point_count + 1U);
        }
        ++packed_index;
    }
    return pixels_changed || rebind_lines;
}

void route_motion_tick(lv_timer_t *) {
    if(ui.nav_route_target_count < 2 || ui.nav_route_point_count < 2) return;
    // 0.39 at 25 ms has approximately the same smoothing time constant as
    // the old 0.56 at 40 ms, but supplies smaller and more frequent steps.
    // Screen coordinates need subpixel precision, not geographic doubles.
    // Float uses the ESP32-S3 FPU for every point in this 40 Hz hot loop.
    const float blend = ui.reduce_motion ? 1.0F : 0.39F;
    bool route_moved = false;
    for(std::uint8_t i = 0; i < ui.nav_route_point_count; ++i) {
        const float dx = ui.nav_route_target_x[i] - ui.nav_route_x[i];
        const float dy = ui.nav_route_target_y[i] - ui.nav_route_y[i];
        if(std::abs(dx) < 0.08F && std::abs(dy) < 0.08F) {
            ui.nav_route_x[i] = ui.nav_route_target_x[i];
            ui.nav_route_y[i] = ui.nav_route_target_y[i];
            continue;
        }
        ui.nav_route_x[i] += dx * blend;
        ui.nav_route_y[i] += dy * blend;
        route_moved = true;
    }
    bool roads_moved = false;
    for(std::uint8_t i = 0; i < ui.nav_road_point_count; ++i) {
        const float dx = ui.nav_road_target_x[i] - ui.nav_road_x[i];
        const float dy = ui.nav_road_target_y[i] - ui.nav_road_y[i];
        if(std::abs(dx) < 0.08F && std::abs(dy) < 0.08F) {
            ui.nav_road_x[i] = ui.nav_road_target_x[i];
            ui.nav_road_y[i] = ui.nav_road_target_y[i];
            continue;
        }
        ui.nav_road_x[i] += dx * blend;
        ui.nav_road_y[i] += dy * blend;
        roads_moved = true;
    }
    bool buildings_moved = false;
    for(std::uint8_t i = 0; i < ui.nav_building_point_count; ++i) {
        const float dx = ui.nav_building_target_x[i] - ui.nav_building_x[i];
        const float dy = ui.nav_building_target_y[i] - ui.nav_building_y[i];
        if(std::abs(dx) < 0.08F && std::abs(dy) < 0.08F) {
            ui.nav_building_x[i] = ui.nav_building_target_x[i];
            ui.nav_building_y[i] = ui.nav_building_target_y[i];
            continue;
        }
        ui.nav_building_x[i] += dx * blend;
        ui.nav_building_y[i] += dy * blend;
        buildings_moved = true;
    }
    // The line objects retain pointers to the mutable point arrays. Ordinary
    // animation therefore only changes those arrays and invalidates the one
    // common map layer. Rebinding every road/building used to invalidate the
    // same 466x232 region dozens of times per frame.
    bool pixels_changed = false;
    if(buildings_moved) {
        pixels_changed = apply_building_geometry_frame(false) || pixels_changed;
    }
    if(roads_moved) {
        pixels_changed = apply_road_geometry_frame(false) || pixels_changed;
    }
    if(route_moved) {
        pixels_changed = apply_route_geometry_frame(false) || pixels_changed;
    }
    if(pixels_changed) lv_obj_invalidate(ui.nav_map);
}

void update_road_geometry(const moto_ui_state_t *state) {
    const std::uint8_t point_count = std::min<std::uint8_t>(
        state->road_point_count, MOTO_UI_ROAD_POINT_CAPACITY);
    const std::uint8_t polyline_count = std::min<std::uint8_t>(
        state->road_polyline_count, MOTO_UI_ROAD_POLYLINE_CAPACITY);
    if(point_count < 2 || polyline_count == 0) {
        ui.nav_road_point_count = 0;
        ui.nav_road_polyline_count = 0;
        ui.nav_road_scene_revision = state->map_scene_revision;
        for(lv_obj_t *road : ui.nav_roads) {
            lv_obj_add_flag(road, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    bool spans_changed =
        ui.nav_road_scene_revision != state->map_scene_revision ||
        ui.nav_road_point_count != point_count ||
        ui.nav_road_polyline_count != polyline_count;
    for(std::uint8_t i = 0; i < point_count; ++i) {
        ui.nav_road_target_x[i] = state->road_points[i].x;
        ui.nav_road_target_y[i] = state->road_points[i].y;
    }
    for(std::uint8_t i = 0; i < polyline_count; ++i) {
        const moto_ui_polyline_span_t next = state->road_polylines[i];
        spans_changed = spans_changed ||
                        ui.nav_road_spans[i].first_point_index !=
                            next.first_point_index ||
                        ui.nav_road_spans[i].point_count != next.point_count ||
                        ui.nav_road_spans[i].road_class != next.road_class;
        ui.nav_road_spans[i] = next;
    }

    // A wide point can move more than 96 px during a perfectly ordinary fast
    // yaw. Treating screen-space distance as a route replacement made the
    // whole map jump. Only topology/revision changes snap; motion interpolates.
    const bool snap = spans_changed || ui.reduce_motion;
    ui.nav_road_point_count = point_count;
    ui.nav_road_polyline_count = polyline_count;
    ui.nav_road_scene_revision = state->map_scene_revision;
    if(snap) {
        for(std::uint8_t i = 0; i < point_count; ++i) {
            ui.nav_road_x[i] = ui.nav_road_target_x[i];
            ui.nav_road_y[i] = ui.nav_road_target_y[i];
        }
    }
    if(spans_changed) {
        for(std::uint8_t i = 0; i < MOTO_UI_ROAD_POLYLINE_CAPACITY; ++i) {
            if(i < polyline_count) {
                const std::uint8_t road_class = ui.nav_road_spans[i].road_class;
                const bool major = road_class <= 2U;
                const bool service = road_class >= 4U;
                lv_obj_set_style_line_width(
                    ui.nav_roads[i], px(major ? 4 : (service ? 2 : 3)), 0);
                lv_obj_set_style_line_color(
                    ui.nav_roads[i],
                    major ? kRoadMajor : (service ? kRoadMinor : kRoadGray), 0);
                lv_obj_remove_flag(ui.nav_roads[i], LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(ui.nav_roads[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
    if(snap) apply_road_geometry_frame();
}


void update_building_geometry(const moto_ui_state_t *state) {
    const std::uint8_t point_count = std::min<std::uint8_t>(
        state->building_point_count, MOTO_UI_BUILDING_POINT_CAPACITY);
    const std::uint8_t footprint_count = std::min<std::uint8_t>(
        state->building_footprint_count,
        MOTO_UI_BUILDING_FOOTPRINT_CAPACITY);
    if(point_count < 3 || footprint_count == 0) {
        ui.nav_building_point_count = 0;
        ui.nav_building_footprint_count = 0;
        ui.nav_building_scene_revision = state->map_scene_revision;
        for(lv_obj_t *building : ui.nav_buildings) {
            lv_obj_add_flag(building, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    bool spans_changed =
        ui.nav_building_scene_revision != state->map_scene_revision ||
        ui.nav_building_point_count != point_count ||
        ui.nav_building_footprint_count != footprint_count;
    for(std::uint8_t i = 0; i < point_count; ++i) {
        ui.nav_building_target_x[i] = state->building_points[i].x;
        ui.nav_building_target_y[i] = state->building_points[i].y;
    }
    for(std::uint8_t i = 0; i < footprint_count; ++i) {
        const moto_ui_building_span_t next = state->building_footprints[i];
        spans_changed = spans_changed ||
                        ui.nav_building_spans[i].first_point_index !=
                            next.first_point_index ||
                        ui.nav_building_spans[i].point_count !=
                            next.point_count ||
                        ui.nav_building_spans[i].building_class !=
                            next.building_class;
        ui.nav_building_spans[i] = next;
    }

    const bool snap = spans_changed || ui.reduce_motion;
    ui.nav_building_point_count = point_count;
    ui.nav_building_footprint_count = footprint_count;
    ui.nav_building_scene_revision = state->map_scene_revision;
    if(snap) {
        for(std::uint8_t i = 0; i < point_count; ++i) {
            ui.nav_building_x[i] = ui.nav_building_target_x[i];
            ui.nav_building_y[i] = ui.nav_building_target_y[i];
        }
    }
    if(spans_changed) {
        for(std::uint8_t i = 0;
            i < MOTO_UI_BUILDING_FOOTPRINT_CAPACITY; ++i) {
            if(i < footprint_count) {
                const bool landmark =
                    ui.nav_building_spans[i].building_class == 1U;
                lv_obj_set_style_line_color(
                    ui.nav_buildings[i],
                    landmark ? kBuildingLandmark : kBuildingGray, 0);
                lv_obj_set_style_line_width(
                    ui.nav_buildings[i], px(landmark ? 2 : 1), 0);
                lv_obj_remove_flag(ui.nav_buildings[i], LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(ui.nav_buildings[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
    if(snap) apply_building_geometry_frame();
}

void update_route_geometry(const moto_ui_state_t *state) {
    const std::uint8_t count = std::min<std::uint8_t>(
        state->route_point_count, MOTO_UI_ROUTE_POINT_CAPACITY);
    if(count < 2) {
        ui.nav_route_point_count = 0;
        ui.nav_route_target_count = 0;
        ui.nav_route_identity = state->route_identity;
        ui.nav_route_generation = state->route_generation;
        lv_obj_add_flag(ui.nav_route_shadow, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui.nav_route, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui.nav_marker, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    for(std::uint8_t i = 0; i < count; ++i) {
        ui.nav_route_target_x[i] = state->route_points[i].x;
        ui.nav_route_target_y[i] = state->route_points[i].y;
    }

    // A new route or a wholesale reroute should appear immediately. Ordinary
    // GNSS/IMU updates are blended at 40 Hz so the road glides under the fixed
    // rider marker instead of jumping from one phone fix to the next.
    const bool snap = ui.nav_route_identity != state->route_identity ||
                      ui.nav_route_generation != state->route_generation ||
                      ui.nav_route_point_count != count || ui.reduce_motion;
    ui.nav_route_target_count = count;
    ui.nav_route_point_count = count;
    ui.nav_route_identity = state->route_identity;
    ui.nav_route_generation = state->route_generation;
    if(snap) {
        for(std::uint8_t i = 0; i < count; ++i) {
            ui.nav_route_x[i] = ui.nav_route_target_x[i];
            ui.nav_route_y[i] = ui.nav_route_target_y[i];
        }
    }

    lv_obj_remove_flag(ui.nav_route_shadow, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(ui.nav_route, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(ui.nav_marker, LV_OBJ_FLAG_HIDDEN);
    if(snap) apply_route_geometry_frame();
}

lv_point_precise_t arrow_base(const lv_point_precise_t& previous,
                              const lv_point_precise_t& tip,
                              double head_length = px(24.0)) {
    const double dx = static_cast<double>(tip.x - previous.x);
    const double dy = static_cast<double>(tip.y - previous.y);
    const double length = std::max(1.0, std::hypot(dx, dy));
    const double ux = dx / length;
    const double uy = dy / length;
    return {
        static_cast<lv_value_precise_t>(tip.x - ux * head_length),
        static_cast<lv_value_precise_t>(tip.y - uy * head_length),
    };
}

void draw_arrowhead(lv_layer_t *layer, const lv_area_t& area,
                    const lv_point_precise_t& previous,
                    const lv_point_precise_t& tip, double icon_scale = 1.0) {
    const double dx = static_cast<double>(tip.x - previous.x);
    const double dy = static_cast<double>(tip.y - previous.y);
    const double length = std::max(1.0, std::hypot(dx, dy));
    const double ux = dx / length;
    const double uy = dy / length;
    const auto base = arrow_base(previous, tip, px(24.0) * icon_scale);
    const double wing = px(18.0) * icon_scale;

    lv_draw_triangle_dsc_t triangle;
    lv_draw_triangle_dsc_init(&triangle);
    triangle.color = kWhite;
    triangle.opa = LV_OPA_COVER;
    triangle.p[0] = {
        static_cast<lv_value_precise_t>(area.x1 + tip.x),
        static_cast<lv_value_precise_t>(area.y1 + tip.y),
    };
    triangle.p[1] = {
        static_cast<lv_value_precise_t>(area.x1 + base.x - uy * wing),
        static_cast<lv_value_precise_t>(area.y1 + base.y + ux * wing),
    };
    triangle.p[2] = {
        static_cast<lv_value_precise_t>(area.x1 + base.x + uy * wing),
        static_cast<lv_value_precise_t>(area.y1 + base.y - ux * wing),
    };
    lv_draw_triangle(layer, &triangle);
}

void draw_maneuver_icon(lv_event_t *event) {
    lv_obj_t *object = lv_event_get_target_obj(event);
    lv_area_t area;
    lv_obj_get_coords(object, &area);
    lv_layer_t *layer = lv_event_get_layer(event);

    const double icon_scale = std::min(
        static_cast<double>(lv_area_get_width(&area)) / px(116),
        static_cast<double>(lv_area_get_height(&area)) / px(108));
    const auto icon_px = [icon_scale](double value) {
        return static_cast<int>(std::lround(px(value) * icon_scale));
    };
    lv_point_precise_t points[8]{};
    int count = 0;
    switch(ui.nav_maneuver_type) {
        case MOTO_MANEUVER_RIGHT:
            points[0] = {icon_px(34), icon_px(102)};
            points[1] = {icon_px(34), icon_px(63)};
            points[2] = {icon_px(55), icon_px(42)};
            points[3] = {icon_px(105), icon_px(42)};
            count = 4;
            break;
        case MOTO_MANEUVER_LEFT:
            points[0] = {icon_px(82), icon_px(102)};
            points[1] = {icon_px(82), icon_px(63)};
            points[2] = {icon_px(61), icon_px(42)};
            points[3] = {icon_px(11), icon_px(42)};
            count = 4;
            break;
        case MOTO_MANEUVER_SLIGHT_RIGHT:
            points[0] = {icon_px(36), icon_px(102)};
            points[1] = {icon_px(36), icon_px(72)};
            points[2] = {icon_px(96), icon_px(12)};
            count = 3;
            break;
        case MOTO_MANEUVER_SLIGHT_LEFT:
            points[0] = {icon_px(80), icon_px(102)};
            points[1] = {icon_px(80), icon_px(72)};
            points[2] = {icon_px(20), icon_px(12)};
            count = 3;
            break;
        case MOTO_MANEUVER_UTURN:
            points[0] = {icon_px(88), icon_px(102)};
            points[1] = {icon_px(88), icon_px(50)};
            points[2] = {icon_px(82), icon_px(33)};
            points[3] = {icon_px(69), icon_px(23)};
            points[4] = {icon_px(51), icon_px(20)};
            points[5] = {icon_px(34), icon_px(27)};
            points[6] = {icon_px(25), icon_px(43)};
            points[7] = {icon_px(25), icon_px(84)};
            count = 8;
            break;
        case MOTO_MANEUVER_ROUNDABOUT: {
            lv_draw_line_dsc_t stem;
            lv_draw_line_dsc_init(&stem);
            stem.color = kWhite;
            stem.width = icon_px(11);
            stem.round_start = 1;
            stem.round_end = 1;
            stem.p1 = {static_cast<lv_value_precise_t>(area.x1 + icon_px(58)),
                       static_cast<lv_value_precise_t>(area.y1 + icon_px(101))};
            stem.p2 = {static_cast<lv_value_precise_t>(area.x1 + icon_px(58)),
                       static_cast<lv_value_precise_t>(area.y1 + icon_px(79))};
            lv_draw_line(layer, &stem);

            lv_draw_arc_dsc_t circle;
            lv_draw_arc_dsc_init(&circle);
            circle.color = kWhite;
            circle.width = icon_px(11);
            circle.rounded = 1;
            circle.center = {
                static_cast<int32_t>(area.x1 + icon_px(58)),
                static_cast<int32_t>(area.y1 + icon_px(53)),
            };
            circle.radius = icon_px(27);
            circle.start_angle = 86;
            circle.end_angle = 326;
            lv_draw_arc(layer, &circle);

            const lv_point_precise_t previous = {icon_px(69), icon_px(24)};
            const lv_point_precise_t tip = {icon_px(86), icon_px(36)};
            draw_arrowhead(layer, area, previous, tip, icon_scale);
            return;
        }
        case MOTO_MANEUVER_ARRIVE: {
            lv_draw_line_dsc_t pole;
            lv_draw_line_dsc_init(&pole);
            pole.color = kWhite;
            pole.width = icon_px(11);
            pole.round_start = 1;
            pole.round_end = 1;
            pole.p1 = {static_cast<lv_value_precise_t>(area.x1 + icon_px(42)),
                       static_cast<lv_value_precise_t>(area.y1 + icon_px(101))};
            pole.p2 = {static_cast<lv_value_precise_t>(area.x1 + icon_px(42)),
                       static_cast<lv_value_precise_t>(area.y1 + icon_px(17))};
            lv_draw_line(layer, &pole);

            lv_draw_triangle_dsc_t flag;
            lv_draw_triangle_dsc_init(&flag);
            flag.color = kWhite;
            flag.opa = LV_OPA_COVER;
            flag.p[0] = {
                static_cast<lv_value_precise_t>(area.x1 + icon_px(46)),
                static_cast<lv_value_precise_t>(area.y1 + icon_px(20)),
            };
            flag.p[1] = {
                static_cast<lv_value_precise_t>(area.x1 + icon_px(102)),
                static_cast<lv_value_precise_t>(area.y1 + icon_px(39)),
            };
            flag.p[2] = {
                static_cast<lv_value_precise_t>(area.x1 + icon_px(46)),
                static_cast<lv_value_precise_t>(area.y1 + icon_px(58)),
            };
            lv_draw_triangle(layer, &flag);
            return;
        }
        case MOTO_MANEUVER_STRAIGHT:
        default:
            points[0] = {icon_px(58), icon_px(103)};
            points[1] = {icon_px(58), icon_px(10)};
            count = 2;
            break;
    }

    lv_point_precise_t absolute[8]{};
    const lv_point_precise_t base = arrow_base(points[count - 2],
                                                points[count - 1], icon_px(24));
    for(int i = 0; i < count; ++i) {
        const lv_point_precise_t point = i == count - 1 ? base : points[i];
        absolute[i] = {
            static_cast<lv_value_precise_t>(area.x1 + point.x),
            static_cast<lv_value_precise_t>(area.y1 + point.y),
        };
    }
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color = kWhite;
    line.width = icon_px(12);
    line.round_start = 1;
    line.round_end = 1;
    line.points = absolute;
    line.point_cnt = count;
    lv_draw_line(layer, &line);
    draw_arrowhead(layer, area, points[count - 2], points[count - 1], icon_scale);
}

void update_maneuver(moto_maneuver_t maneuver) {
    if(ui.nav_maneuver_type == maneuver) return;
    ui.nav_maneuver_type = maneuver;
    lv_obj_invalidate(ui.nav_maneuver);
}

void draw_segment(lv_layer_t *layer, lv_color_t color, int width,
                  lv_opa_t opacity, const lv_point_precise_t *points,
                  int count) {
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color = color;
    line.width = width;
    line.opa = opacity;
    line.round_start = 1;
    line.round_end = 1;
    // LVGL's descriptor predates const-correct polyline inputs; the draw task
    // only reads the points for the duration of this call.
    line.points = const_cast<lv_point_precise_t *>(points);
    line.point_cnt = count;
    lv_draw_line(layer, &line);
}

void draw_disc(lv_layer_t *layer, int x, int y, int diameter,
               lv_color_t color, lv_opa_t opacity) {
    lv_draw_rect_dsc_t disc;
    lv_draw_rect_dsc_init(&disc);
    disc.radius = LV_RADIUS_CIRCLE;
    disc.bg_color = color;
    disc.bg_opa = opacity;
    disc.border_opa = LV_OPA_TRANSP;
    const lv_area_t area = {
        x - diameter / 2,
        y - diameter / 2,
        x + diameter / 2,
        y + diameter / 2,
    };
    lv_draw_rect(layer, &disc, &area);
}

void draw_phone_body(lv_layer_t *layer, const lv_area_t& area,
                     lv_color_t color, lv_opa_t opacity) {
    const lv_area_t phone = {
        area.x1 + px(49),
        area.y1 + px(25),
        area.x1 + px(101),
        area.y1 + px(121),
    };
    lv_draw_rect_dsc_t frame;
    lv_draw_rect_dsc_init(&frame);
    frame.radius = px(13);
    frame.bg_opa = LV_OPA_TRANSP;
    frame.border_color = color;
    frame.border_width = px(4);
    frame.border_opa = opacity;
    lv_draw_rect(layer, &frame, &phone);

    const lv_point_precise_t receiver[] = {
        {static_cast<lv_value_precise_t>(area.x1 + px(67)),
         static_cast<lv_value_precise_t>(area.y1 + px(36))},
        {static_cast<lv_value_precise_t>(area.x1 + px(83)),
         static_cast<lv_value_precise_t>(area.y1 + px(36))},
    };
    const lv_point_precise_t home[] = {
        {static_cast<lv_value_precise_t>(area.x1 + px(68)),
         static_cast<lv_value_precise_t>(area.y1 + px(110))},
        {static_cast<lv_value_precise_t>(area.x1 + px(82)),
         static_cast<lv_value_precise_t>(area.y1 + px(110))},
    };
    draw_segment(layer, color, px(3), opacity, receiver, 2);
    draw_segment(layer, color, px(3), opacity, home, 2);
}

void draw_connection_symbol(lv_event_t *event) {
    lv_obj_t *object = lv_event_get_target_obj(event);
    lv_area_t area;
    lv_obj_get_coords(object, &area);
    lv_layer_t *layer = lv_event_get_layer(event);
    const int center_x = area.x1 + px(75);
    const int center_y = area.y1 + px(73);
    const double phase = ui.lifecycle_phase_deg * kPi / 180.0;

    if(ui.lifecycle_visual == LifecycleVisual::Connected) {
        const int pulse = static_cast<int>(px(67) +
            std::lround((std::sin(phase) + 1.0) * px(3.0)));
        lv_draw_arc_dsc_t halo;
        lv_draw_arc_dsc_init(&halo);
        halo.color = kGreen;
        halo.width = px(3);
        halo.opa = LV_OPA_50;
        halo.rounded = 1;
        halo.center = {center_x, center_y};
        halo.radius = pulse;
        halo.start_angle = 0;
        halo.end_angle = 360;
        lv_draw_arc(layer, &halo);
        draw_disc(layer, center_x, center_y, px(92), kGreen, LV_OPA_COVER);
        const lv_point_precise_t check[] = {
            {static_cast<lv_value_precise_t>(area.x1 + px(53)),
             static_cast<lv_value_precise_t>(area.y1 + px(74))},
            {static_cast<lv_value_precise_t>(area.x1 + px(69)),
             static_cast<lv_value_precise_t>(area.y1 + px(89))},
            {static_cast<lv_value_precise_t>(area.x1 + px(99)),
             static_cast<lv_value_precise_t>(area.y1 + px(55))},
        };
        draw_segment(layer, kBlack, px(9), LV_OPA_COVER, check, 3);
        return;
    }

    if(ui.lifecycle_visual == LifecycleVisual::Ready ||
       ui.lifecycle_visual == LifecycleVisual::Planning ||
       ui.lifecycle_visual == LifecycleVisual::Locating ||
       ui.lifecycle_visual == LifecycleVisual::SyncingRoute) {
        const lv_point_precise_t route[] = {
            {static_cast<lv_value_precise_t>(area.x1 + px(34)),
             static_cast<lv_value_precise_t>(area.y1 + px(113))},
            {static_cast<lv_value_precise_t>(area.x1 + px(50)),
             static_cast<lv_value_precise_t>(area.y1 + px(91))},
            {static_cast<lv_value_precise_t>(area.x1 + px(80)),
             static_cast<lv_value_precise_t>(area.y1 + px(98))},
            {static_cast<lv_value_precise_t>(area.x1 + px(111)),
             static_cast<lv_value_precise_t>(area.y1 + px(48))},
        };
        draw_segment(layer, kGraphite, px(12), LV_OPA_COVER, route, 4);
        draw_segment(layer, kWhite, px(5), LV_OPA_COVER, route, 4);
        draw_disc(layer, route[0].x, route[0].y, px(16), kWhite,
                  LV_OPA_COVER);

        lv_draw_arc_dsc_t pin;
        lv_draw_arc_dsc_init(&pin);
        pin.color = kWhite;
        pin.width = px(5);
        pin.opa = LV_OPA_COVER;
        pin.rounded = 1;
        pin.center = {route[3].x, route[3].y};
        pin.radius = px(13);
        pin.start_angle = 0;
        pin.end_angle = 360;
        lv_draw_arc(layer, &pin);
        draw_disc(layer, route[3].x, route[3].y, px(7), kWhite,
                  LV_OPA_COVER);

        if(ui.lifecycle_visual != LifecycleVisual::Ready) {
            // A single travelling bead communicates progress without a
            // processor-heavy full-screen animation.
            const double t =
                static_cast<double>(ui.lifecycle_phase_deg % 120U) / 120.0;
            const int segment = std::min(2, static_cast<int>(t * 3.0));
            const double local = t * 3.0 - segment;
            const int x = static_cast<int>(std::lround(
                route[segment].x +
                (route[segment + 1].x - route[segment].x) * local));
            const int y = static_cast<int>(std::lround(
                route[segment].y +
                (route[segment + 1].y - route[segment].y) * local));
            draw_disc(layer, x, y, px(12), kWhite, LV_OPA_COVER);
        } else {
            const lv_opa_t ready_opa = static_cast<lv_opa_t>(
                150 + std::lround((std::sin(phase) + 1.0) * 40.0));
            draw_disc(layer, route[0].x, route[0].y, px(25), kWhite,
                      ready_opa);
        }
        return;
    }

    draw_phone_body(layer, area, kWhite, LV_OPA_COVER);
    const bool connecting =
        ui.lifecycle_visual == LifecycleVisual::PhoneConnecting;
    const int speed = connecting ? 2 : 1;
    for(int index = 0; index < 3; ++index) {
        lv_draw_arc_dsc_t scan;
        lv_draw_arc_dsc_init(&scan);
        scan.color = index == 0 ? kWhite : kQuiet;
        scan.width = px(index == 0 ? 4 : 3);
        scan.opa = static_cast<lv_opa_t>(220 - index * 55);
        scan.rounded = 1;
        scan.center = {center_x, center_y};
        scan.radius = px(66 - index * 7);
        const int start =
            (static_cast<int>(ui.lifecycle_phase_deg) * speed + index * 112) %
            360;
        scan.start_angle = start;
        scan.end_angle = start + (connecting ? 54 : 34);
        lv_draw_arc(layer, &scan);
    }
    const int orbit_x = static_cast<int>(
        std::lround(center_x + std::sin(phase * speed) * px(66.0)));
    const int orbit_y = static_cast<int>(
        std::lround(center_y - std::cos(phase * speed) * px(66.0)));
    draw_disc(layer, orbit_x, orbit_y, px(connecting ? 11 : 9), kWhite,
              LV_OPA_COVER);
}

void set_layer_opacity(void *object, int32_t opacity) {
    lv_obj_set_style_opa(static_cast<lv_obj_t *>(object), opacity, 0);
}

void apply_lifecycle_content(LifecycleVisual visual) {
    ui.lifecycle_visual = visual;
    const char *kicker = "MOTO GPS / PHONE LINK";
    const char *title = "";
    const char *subtitle = "";
    lv_color_t title_color = kWhite;
    switch(visual) {
        case LifecycleVisual::PhoneOffline:
            title = "CONNECT PHONE";
            subtitle = "请打开手机应用";
            break;
        case LifecycleVisual::PhoneConnecting:
            title = "CONNECTING";
            subtitle = "正在建立连接";
            break;
        case LifecycleVisual::Connected:
            title = "CONNECTED";
            subtitle = "连接成功";
            title_color = kGreen;
            break;
        case LifecycleVisual::Ready:
            kicker = "MOTO GPS / READY";
            title = "READY TO RIDE";
            subtitle = "请在手机选择目的地";
            break;
        case LifecycleVisual::Planning:
            kicker = "MOTO GPS / ROUTE";
            title = "BUILDING ROUTE";
            subtitle = "正在规划路线";
            break;
        case LifecycleVisual::Locating:
            kicker = "MOTO GPS / NAVIGATION";
            title = "WAITING FOR GPS";
            subtitle = "等待手机定位";
            break;
        case LifecycleVisual::SyncingRoute:
            kicker = "MOTO GPS / NAVIGATION";
            title = "SYNCING ROUTE";
            subtitle = "正在获取路线";
            break;
        case LifecycleVisual::Hidden:
            break;
    }
    lv_label_set_text(ui.nav_lifecycle_kicker, kicker);
    lv_label_set_text(ui.nav_lifecycle_title, title);
    lv_label_set_text(ui.nav_lifecycle_subtitle, subtitle);
    lv_obj_set_style_text_color(ui.nav_lifecycle_title, title_color, 0);
    lv_obj_set_style_text_color(ui.nav_lifecycle_subtitle,
                                visual == LifecycleVisual::Connected
                                    ? kGreen : kSoft,
                                0);
    // Refresh the whole overlay when its meaning changes. Otherwise a partial
    // flush can clear unchanged header/icon pixels during a state transition.
    lv_obj_invalidate(ui.nav_lifecycle);
}

void lifecycle_fade_in();

void lifecycle_fade_out_complete(lv_anim_t *) {
    if(ui.nav_lifecycle == nullptr) return;
    if(ui.lifecycle_target == LifecycleVisual::Hidden) {
        ui.lifecycle_visual = LifecycleVisual::Hidden;
        lv_obj_add_flag(ui.nav_lifecycle, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_opa(ui.nav_lifecycle, LV_OPA_COVER, 0);
        return;
    }
    apply_lifecycle_content(ui.lifecycle_target);
    lifecycle_fade_in();
}

void lifecycle_fade_in() {
    lv_obj_remove_flag(ui.nav_lifecycle, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(ui.nav_lifecycle, LV_OPA_TRANSP, 0);
    lv_anim_t fade;
    lv_anim_init(&fade);
    lv_anim_set_var(&fade, ui.nav_lifecycle);
    lv_anim_set_exec_cb(&fade, set_layer_opacity);
    lv_anim_set_values(&fade, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_duration(&fade, 220);
    lv_anim_set_path_cb(&fade, lv_anim_path_ease_out);
    lv_anim_start(&fade);
}

void transition_lifecycle(LifecycleVisual next) {
    if(ui.nav_lifecycle == nullptr) return;
    if(next == ui.lifecycle_target &&
       (next == ui.lifecycle_visual ||
        lv_obj_has_flag(ui.nav_lifecycle, LV_OBJ_FLAG_HIDDEN))) {
        return;
    }
    ui.lifecycle_target = next;
    lv_anim_delete(ui.nav_lifecycle, set_layer_opacity);
    if(ui.reduce_motion) {
        if(next == LifecycleVisual::Hidden) {
            ui.lifecycle_visual = LifecycleVisual::Hidden;
            lv_obj_add_flag(ui.nav_lifecycle, LV_OBJ_FLAG_HIDDEN);
        } else {
            apply_lifecycle_content(next);
            lv_obj_remove_flag(ui.nav_lifecycle, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_opa(ui.nav_lifecycle, LV_OPA_COVER, 0);
        }
        return;
    }
    if(ui.lifecycle_visual == LifecycleVisual::Hidden ||
       lv_obj_has_flag(ui.nav_lifecycle, LV_OBJ_FLAG_HIDDEN)) {
        if(next == LifecycleVisual::Hidden) return;
        apply_lifecycle_content(next);
        lifecycle_fade_in();
        return;
    }

    lv_anim_t fade;
    lv_anim_init(&fade);
    lv_anim_set_var(&fade, ui.nav_lifecycle);
    lv_anim_set_exec_cb(&fade, set_layer_opacity);
    lv_anim_set_values(&fade,
                       lv_obj_get_style_opa(ui.nav_lifecycle, LV_PART_MAIN),
                       LV_OPA_TRANSP);
    lv_anim_set_duration(&fade, 140);
    lv_anim_set_path_cb(&fade, lv_anim_path_ease_in);
    lv_anim_set_completed_cb(&fade, lifecycle_fade_out_complete);
    lv_anim_start(&fade);
}

LifecycleVisual desired_lifecycle_visual() {
    if(ui.demo_active) {
        return LifecycleVisual::Hidden;
    }
    if(ui.phone_connection == MOTO_UI_PHONE_OFFLINE) {
        return LifecycleVisual::PhoneOffline;
    }
    if(ui.phone_connection == MOTO_UI_PHONE_CONNECTING) {
        return LifecycleVisual::PhoneConnecting;
    }
    if(ui.lifecycle_success_active) {
        return LifecycleVisual::Connected;
    }
    if(ui.navigation_has_route_display) {
        return LifecycleVisual::Hidden;
    }
    if(ui.navigation_route_request_in_flight) {
        return LifecycleVisual::Planning;
    }
    // A missing BLE geometry window is not an ended navigation session.
    // Keep the destination alive through stale fixes and route replacement.
    if(ui.navigation_has_destination) {
        return ui.navigation_has_usable_fix
                   ? LifecycleVisual::SyncingRoute
                   : LifecycleVisual::Locating;
    }
    return LifecycleVisual::Ready;
}

void refresh_lifecycle() {
    transition_lifecycle(desired_lifecycle_visual());
}

void lifecycle_tick(lv_timer_t *) {
    if(ui.reduce_motion || ui.nav_lifecycle_symbol == nullptr ||
       ui.lifecycle_visual == LifecycleVisual::Hidden) {
        return;
    }
    const std::uint16_t step =
        ui.lifecycle_visual == LifecycleVisual::PhoneConnecting ? 9U : 5U;
    ui.lifecycle_phase_deg =
        static_cast<std::uint16_t>((ui.lifecycle_phase_deg + step) % 360U);
    lv_obj_invalidate(ui.nav_lifecycle_symbol);
}

void connection_success_timeout(lv_timer_t *timer) {
    lv_timer_pause(timer);
    ui.lifecycle_success_active = false;
    refresh_lifecycle();
}

void set_navigation_guidance_visible(bool visible) {
    lv_obj_t *objects[] = {
        ui.nav_maneuver,
        ui.nav_distance,
        ui.nav_unit,
        ui.nav_progress,
    };
    for(lv_obj_t *object : objects) {
        if(visible) {
            lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if(!visible) lv_obj_add_flag(ui.nav_limit, LV_OBJ_FLAG_HIDDEN);
}

void update_nav_status(const moto_ui_state_t *state) {
    const char *text = "";
    lv_color_t color = kQuiet;
    lv_obj_set_style_text_font(ui.nav_status, &ducati_label_18, 0);
    if(ui.demo_active) {
        lv_label_set_text(ui.nav_status, "DEMO RIDE");
        lv_obj_set_style_text_color(ui.nav_status, kWhite, 0);
        return;
    }
    if(state->mode != MOTO_UI_ARRIVED && state->route_point_count < 2) {
        // Empty navigation states are rendered by the full lifecycle surface,
        // never by a tiny diagnostic caption floating in a black screen.
        lv_label_set_text(ui.nav_status, "");
        return;
    }
    if(state->has_destination && !state->has_usable_fix &&
       state->mode != MOTO_UI_ARRIVED && state->mode != MOTO_UI_REROUTING) {
        lv_obj_set_style_text_font(ui.nav_status, &moto_font_nav_16, 0);
        lv_label_set_text(ui.nav_status, "等待定位");
        lv_obj_set_style_text_color(ui.nav_status, kAmber, 0);
        return;
    }
    switch(state->mode) {
        case MOTO_UI_ACQUIRING_FIX: text = "ACQUIRING GPS"; color = kAmber; break;
        case MOTO_UI_REROUTING: text = "REROUTING"; color = kAmber; break;
        case MOTO_UI_OFFLINE: text = "ROUTE CACHED"; color = kQuiet; break;
        case MOTO_UI_ARRIVED: text = "ARRIVED"; color = kGreen; break;
        case MOTO_UI_NAVIGATING:
            if(state->online == 0) text = "NO HOTSPOT";
            break;
    }
    lv_label_set_text(ui.nav_status, text);
    lv_obj_set_style_text_color(ui.nav_status, color, 0);
}

void draw_lean_graphic(lv_event_t *event) {
    const auto *graphic = static_cast<const LeanGraphic *>(lv_event_get_user_data(event));
    lv_obj_t *object = lv_event_get_target_obj(event);
    lv_area_t area;
    lv_obj_get_coords(object, &area);
    lv_layer_t *layer = lv_event_get_layer(event);
    const double width = lv_area_get_width(&area);
    const double height = lv_area_get_height(&area);
    // All strokes stay inside this object's bounds even at +/-90 degrees.
    const double scale = std::min(width / 70.0, height / 44.0);
    const double pivot_x = area.x1 + width / 2.0;
    const double pivot_y = area.y1 + height * 0.78;
    const auto point = [&](double x, double y) -> lv_point_precise_t {
        return {
            static_cast<lv_value_precise_t>(pivot_x + scale * x),
            static_cast<lv_value_precise_t>(pivot_y + scale * y),
        };
    };
    const auto segment = [&](double x1, double y1, double x2, double y2,
                              lv_color_t color, double stroke) {
        const lv_point_precise_t points[] = {point(x1, y1), point(x2, y2)};
        draw_segment(layer, color, std::max(1, static_cast<int>(std::lround(stroke * scale))),
                      LV_OPA_COVER, points, 2);
    };
    // Fixed ground and upright reference: these never follow the motorcycle.
    segment(-25, 1, 25, 1, kGraphite, 1.0);
    for(int y = -34; y < 0; y += 5)
        segment(0, y, 0, y + 2, kGraphite, 1.0);
    if(!graphic->valid) {
        // No upright motorcycle is shown when the sensor has no valid angle.
        segment(-3, -18, 3, -12, kQuiet, 1.5);
        segment(-3, -12, 3, -18, kQuiet, 1.5);
        return;
    }

    // Approved E artwork: preserve the red tail, mirrors, twin exhausts and
    // dark rear tire. One small flash bitmap rotates around its contact point.
    // Fit the complete rotation envelope, including interpolation at +/-90°.
    const double image_scale = std::min({
        (width / 2.0 - 3.0) / DUCATI_REAR_E_RADIUS,
        (pivot_y - area.y1 - 3.0) / DUCATI_REAR_E_PIVOT_Y,
        (area.y2 - pivot_y - 2.0) / DUCATI_REAR_E_PIVOT_X,
    });
    lv_draw_image_dsc_t image;
    lv_draw_image_dsc_init(&image);
    image.src = &ducati_rear_e;
    image.pivot = {DUCATI_REAR_E_PIVOT_X, DUCATI_REAR_E_PIVOT_Y};
    image.rotation = (static_cast<int>(std::lround(graphic->angle_deg * 10.0F)) + 3600) % 3600;
    image.scale_x = image.scale_y = std::max(1, static_cast<int>(std::floor(image_scale * LV_SCALE_NONE)));
    image.antialias = 1;
    lv_area_t image_area;
    image_area.x1 = static_cast<int>(std::lround(pivot_x)) - DUCATI_REAR_E_PIVOT_X;
    image_area.y1 = static_cast<int>(std::lround(pivot_y)) - DUCATI_REAR_E_PIVOT_Y;
    image_area.x2 = image_area.x1 + DUCATI_REAR_E_WIDTH - 1;
    image_area.y2 = image_area.y1 + DUCATI_REAR_E_HEIGHT - 1;
    lv_draw_image(layer, &image, &image_area);
}

void update_lean(lv_obj_t *label, LeanGraphic& graphic, const moto_ui_state_t *state) {
    if(label == nullptr) return;
    const bool valid = state->lean_valid && std::isfinite(state->lean_deg) &&
                       std::abs(state->lean_deg) <= 90.0F;
    char text[20] = "--°";
    if(valid) {
        const int angle = static_cast<int>(std::lround(std::abs(state->lean_deg)));
        if(angle == 0) std::snprintf(text, sizeof(text), "0°");
        else std::snprintf(text, sizeof(text), "%s %d°",
                           state->lean_deg < 0.0F ? "L" : "R", angle);
    }
    const bool text_changed = std::strcmp(lv_label_get_text(label), text) != 0;
    const bool validity_changed = graphic.valid != valid;
    if(text_changed) lv_label_set_text(label, text);
    if(validity_changed) lv_obj_set_style_text_color(label, valid ? kWhite : kQuiet, 0);
    const float angle = valid ? state->lean_deg : 0.0F;
    if(graphic.object != nullptr && (graphic.valid != valid || graphic.angle_deg != angle)) {
        graphic.valid = valid;
        graphic.angle_deg = angle;
        lv_obj_invalidate(graphic.object);
        if(graphic.reference != nullptr) lv_obj_invalidate(graphic.reference);
        // Clear the complete local readout (old glyphs and rotated tail) before
        // a synchronous hardware refresh, without dirtying the map or screen.
        lv_obj_invalidate(lv_obj_get_parent(label));
    }
}

void lean_calibration_event(lv_event_t *) {
    if(ui.lean_calibration_callback != nullptr)
        ui.lean_calibration_callback(ui.lean_calibration_context);
}

#if MOTO_UI_SQUARE_SCREEN
lv_obj_t *create_lean_readout(lv_obj_t *parent, LeanGraphic& graphic,
                              int x, int y, bool compact) {
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_set_pos(panel, px(x), px(y));
    lv_obj_set_size(panel, px(compact ? 132 : 190), px(compact ? 53 : 78));
    lv_obj_set_style_bg_color(panel, kBlack, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(panel, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, kRed, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(panel, lean_calibration_event, LV_EVENT_LONG_PRESSED, nullptr);

    graphic.object = lv_obj_create(panel);
    lv_obj_remove_style_all(graphic.object);
    lv_obj_set_size(graphic.object, px(compact ? 66 : 94), px(compact ? 53 : 74));
    lv_obj_set_pos(graphic.object, 0, 0);
    lv_obj_remove_flag(graphic.object, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(graphic.object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(graphic.object, draw_lean_graphic, LV_EVENT_DRAW_MAIN_END, &graphic);

    const int text_x = compact ? 73 : 110;
    const int text_width = compact ? 57 : 70;
    lv_obj_t *title = make_label(panel, &ducati_label_18, kRed, "LEAN");
    lv_obj_set_pos(title, px(text_x), px(compact ? 3 : 10));
    lv_obj_set_width(title, px(text_width));
    lv_obj_t *value = make_label(panel, &ducati_readout_32, kQuiet, "--°");
    lv_obj_set_pos(value, px(text_x), px(compact ? 23 : 32));
    lv_obj_set_width(value, px(text_width));
    lv_obj_set_style_text_align(title, compact ? LV_TEXT_ALIGN_LEFT : LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_align(value, compact ? LV_TEXT_ALIGN_LEFT : LV_TEXT_ALIGN_CENTER, 0);
    return value;
}

void draw_nav_lean_reference(lv_event_t *event) {
    const auto *graphic = static_cast<const LeanGraphic *>(lv_event_get_user_data(event));
    lv_area_t area;
    lv_obj_get_coords(lv_event_get_target_obj(event), &area);
    lv_layer_t *layer = lv_event_get_layer(event);
    const int left = area.x1 + px(4);
    const int right = area.x2 - px(4);
    const int baseline = area.y1 + px(8);
    const auto line = [&](int x1, int y1, int x2, int y2, lv_color_t color, int stroke) {
        const lv_point_precise_t points[] = {{static_cast<lv_value_precise_t>(x1),
                                              static_cast<lv_value_precise_t>(y1)},
                                             {static_cast<lv_value_precise_t>(x2),
                                              static_cast<lv_value_precise_t>(y2)}};
        draw_segment(layer, color, stroke, LV_OPA_COVER, points, 2);
    };
    line(left, baseline, right, baseline, kGraphite, px(1));
    for(int quarter = 1; quarter <= 3; ++quarter) {
        const int x = left + (right - left) * quarter / 4;
        line(x, baseline - px(4), x, baseline, kQuiet, px(1));
    }
    if(graphic->valid) {
        const double amount = std::clamp(graphic->angle_deg / 90.0F, -1.0F, 1.0F);
        const int x = static_cast<int>(std::lround((left + right) / 2.0 +
                                                    amount * (right - left) / 2.0));
        line(x, baseline - px(8), x, baseline, kRed, px(2));
    }
}

void create_nav_lean_readout(lv_obj_t *parent) {
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_set_pos(panel, px(212), px(236));
    lv_obj_set_size(panel, px(136), px(96));
    lv_obj_set_style_bg_color(panel, kBlack, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(panel, lean_calibration_event, LV_EVENT_LONG_PRESSED, nullptr);

    ui.nav_lean_graphic.object = lv_obj_create(panel);
    lv_obj_remove_style_all(ui.nav_lean_graphic.object);
    lv_obj_set_pos(ui.nav_lean_graphic.object, 0, 0);
    lv_obj_set_size(ui.nav_lean_graphic.object, px(84), px(88));
    lv_obj_remove_flag(ui.nav_lean_graphic.object, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(ui.nav_lean_graphic.object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ui.nav_lean_graphic.object, draw_lean_graphic,
                        LV_EVENT_DRAW_MAIN_END, &ui.nav_lean_graphic);
    lv_obj_t *title = make_label(panel, &ducati_label_18, kRed, "LEAN");
    lv_obj_set_pos(title, px(82), px(18));
    lv_obj_set_width(title, px(54));
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    ui.nav_lean_value = make_label(panel, &ducati_readout_32, kQuiet, "--°");
    lv_obj_set_pos(ui.nav_lean_value, px(80), px(38));
    lv_obj_set_width(ui.nav_lean_value, px(56));
    lv_obj_set_style_text_align(ui.nav_lean_value, LV_TEXT_ALIGN_CENTER, 0);
    ui.nav_lean_graphic.reference = lv_obj_create(panel);
    lv_obj_remove_style_all(ui.nav_lean_graphic.reference);
    lv_obj_set_pos(ui.nav_lean_graphic.reference, 0, px(85));
    lv_obj_set_size(ui.nav_lean_graphic.reference, px(136), px(11));
    lv_obj_remove_flag(ui.nav_lean_graphic.reference, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui.nav_lean_graphic.reference, draw_nav_lean_reference,
                        LV_EVENT_DRAW_MAIN_END, &ui.nav_lean_graphic);
}

void draw_nav_speedometer(lv_event_t *event) {
    lv_area_t area;
    lv_obj_get_coords(lv_event_get_target_obj(event), &area);
    lv_layer_t *layer = lv_event_get_layer(event);
    const int center_x = area.x1 + px(48);
    const int center_y = area.y1 + px(48);
    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init(&arc);
    arc.center = {center_x, center_y};
    arc.radius = static_cast<std::uint16_t>(px(46));
    arc.width = px(1);
    arc.color = kGraphite;
    arc.start_angle = 138;
    arc.end_angle = 402;
    lv_draw_arc(layer, &arc);
    if(ui.nav_speed_kph > 0) {
        arc.color = kWhite;
        arc.width = px(2);
        arc.rounded = 1;
        arc.end_angle = static_cast<lv_value_precise_t>(
            std::lround(138 + std::min<int>(160, ui.nav_speed_kph) * 264.0 / 160));
        lv_draw_arc(layer, &arc);
    }
    arc.color = kRed;
    arc.width = px(2);
    arc.rounded = 0;
    arc.start_angle = 369;
    arc.end_angle = 402;
    lv_draw_arc(layer, &arc);
    for(int i = 0; i <= 16; ++i) {
        const double angle = (138.0 + i * 264.0 / 16) * kPi / 180.0;
        const double inner = px(i % 4 == 0 ? 38.0 : 40.0);
        const double outer = px(42.0);
        const lv_point_precise_t points[] = {
            {static_cast<lv_value_precise_t>(center_x + std::cos(angle) * inner),
             static_cast<lv_value_precise_t>(center_y + std::sin(angle) * inner)},
            {static_cast<lv_value_precise_t>(center_x + std::cos(angle) * outer),
             static_cast<lv_value_precise_t>(center_y + std::sin(angle) * outer)},
        };
        draw_segment(layer, i >= 14 ? kRed : kWhite, px(1), LV_OPA_COVER, points, 2);
    }
}

void create_nav_speed_readout(lv_obj_t *parent) {
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_set_pos(panel, px(12), px(225));
    lv_obj_set_size(panel, px(96), px(106));
    lv_obj_set_style_bg_color(panel, kBlack, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_GESTURE_BUBBLE);

    lv_obj_t *title = make_label(panel, &ducati_label_18, kRed, "SPEED");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);
    ui.nav_speed_gauge = lv_obj_create(panel);
    lv_obj_remove_style_all(ui.nav_speed_gauge);
    lv_obj_set_pos(ui.nav_speed_gauge, 0, px(17));
    lv_obj_set_size(ui.nav_speed_gauge, px(96), px(89));
    lv_obj_remove_flag(ui.nav_speed_gauge, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui.nav_speed_gauge, draw_nav_speedometer, LV_EVENT_DRAW_MAIN_END, nullptr);

    for(int value = 0; value <= 160; value += 40) {
        const double angle = (138.0 + value * 264.0 / 160) * kPi / 180.0;
        char text[8];
        std::snprintf(text, sizeof(text), "%d", value);
        lv_obj_t *label = make_label(panel, &ducati_label_18, kWhite, text);
        lv_obj_set_size(label, px(26), px(14));
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label,
                        static_cast<int>(std::lround(px(35.0) + std::cos(angle) * px(35.0))),
                        static_cast<int>(std::lround(px(58.0) + std::sin(angle) * px(35.0))));
    }
    ui.nav_speed_value = make_label(panel, &ducati_readout_56, kWhite, "0");
    lv_obj_set_pos(ui.nav_speed_value, 0, px(39));
    lv_obj_set_width(ui.nav_speed_value, px(96));
    lv_obj_set_style_text_align(ui.nav_speed_value, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_t *unit = make_label(panel, &ducati_label_18, kSoft, "km/h");
    lv_obj_align(unit, LV_ALIGN_BOTTOM_MID, 0, px(-2));
}

void update_nav_speed_readout(const moto_ui_state_t *state) {
    const bool available = ui.demo_active || !state->has_destination || state->has_usable_fix;
    if(ui.nav_speed_value == nullptr ||
       (state->speed_kph == ui.nav_speed_kph && available == ui.nav_speed_available)) return;
    ui.nav_speed_kph = state->speed_kph;
    ui.nav_speed_available = available;
    char text[8];
    if(available) std::snprintf(text, sizeof(text), "%u", static_cast<unsigned>(state->speed_kph));
    else std::snprintf(text, sizeof(text), "--");
    lv_label_set_text(ui.nav_speed_value, text);
    lv_obj_invalidate(ui.nav_speed_gauge);
}
#endif

void update_navigation(const moto_ui_state_t *state) {
    update_lean(ui.nav_lean_value, ui.nav_lean_graphic, state);
#if MOTO_UI_SQUARE_SCREEN
    update_nav_speed_readout(state);
#endif
    update_building_geometry(state);
    update_road_geometry(state);
    update_route_geometry(state);
    update_maneuver(state->maneuver);
    update_nav_status(state);

    // A remembered route remains visible without GPS, while live turn/distance
    // readouts wait for a usable fix. Only an empty map needs the full overlay.
    const bool has_route_display = ui.demo_active ||
                                   state->mode == MOTO_UI_ARRIVED ||
                                   state->route_point_count >= 2;
    const bool has_guidance = has_route_display &&
                             (ui.demo_active || state->mode == MOTO_UI_ARRIVED || state->has_usable_fix);
    ui.navigation_has_route_display = has_route_display;
    ui.navigation_has_destination = state->has_destination != 0;
    ui.navigation_has_usable_fix = state->has_usable_fix != 0;
    ui.navigation_route_request_in_flight =
        state->route_request_in_flight != 0;
    set_navigation_guidance_visible(has_guidance);
    if(has_route_display) lv_obj_remove_flag(ui.nav_progress, LV_OBJ_FLAG_HIDDEN);
    refresh_lifecycle();
    if(!has_route_display) return;

    char value[16];
    const char *unit = "m";
    if(state->distance_to_maneuver_m >= 1000) {
        std::snprintf(value, sizeof(value), "%.1f",
                      state->distance_to_maneuver_m / 1000.0);
        unit = "km";
    } else {
        std::snprintf(value, sizeof(value), "%u",
                      static_cast<unsigned>(state->distance_to_maneuver_m));
    }
    if(state->mode == MOTO_UI_ARRIVED) std::snprintf(value, sizeof(value), "0");
    lv_label_set_text(ui.nav_distance, value);
    lv_label_set_text(ui.nav_unit, unit);
    // This number is never the total trip distance. Its physical attachment to
    // the maneuver glyph makes that meaning clear without an explanatory label.
#if MOTO_UI_SQUARE_SCREEN
    lv_obj_set_style_text_font(ui.nav_distance,
                                std::strlen(value) >= 4 ? &ducati_readout_32 : &ducati_readout_56, 0);
    lv_obj_align(ui.nav_distance, LV_ALIGN_TOP_LEFT, px(76), px(25));
#else
    lv_obj_align(ui.nav_distance, LV_ALIGN_TOP_LEFT, px(151), px(244));
#endif
    lv_obj_align_to(ui.nav_unit, ui.nav_distance, LV_ALIGN_OUT_RIGHT_BOTTOM,
                    px(6), px(-7));

#if MOTO_UI_SQUARE_SCREEN
    lv_bar_set_value(ui.nav_progress,
                     std::min<int>(100, state->route_progress_percent), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ui.nav_progress, traffic_color(state->traffic),
                              LV_PART_INDICATOR);
#else
    lv_arc_set_value(ui.nav_progress,
                     std::min<int>(100, state->route_progress_percent));
    lv_obj_set_style_arc_color(ui.nav_progress, traffic_color(state->traffic),
                               LV_PART_INDICATOR);
#endif

    if(has_guidance && state->speed_limit_kph > 0) {
        char limit[8];
        std::snprintf(limit, sizeof(limit), "%u",
                      static_cast<unsigned>(state->speed_limit_kph));
        lv_label_set_text(ui.nav_limit_value, limit);
        lv_obj_remove_flag(ui.nav_limit, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(ui.nav_limit, LV_OBJ_FLAG_HIDDEN);
    }
}

void update_speedometer(const moto_ui_state_t *state) {
    update_lean(ui.speed_lean_value, ui.speed_lean_graphic, state);
    char value[8];
    std::snprintf(value, sizeof(value), "%u",
                  static_cast<unsigned>(state->speed_kph));
    lv_label_set_text(ui.speed_value, value);
    lv_obj_align(ui.speed_value, LV_ALIGN_CENTER, 0, px(-10));
    lv_arc_set_value(ui.speed_arc, std::min<int>(160, state->speed_kph));
    for(int i = 0; i < kSpeedTickCount; ++i) {
        const bool active = state->speed_kph >= static_cast<unsigned>(i * 10);
        lv_obj_set_style_line_color(ui.speed_ticks[i], i >= 14 ? kRed :
                                    (active ? kWhite : kSoft), 0);
    }
}

void update_compass(const moto_ui_state_t *state) {
    char heading[12];
    std::snprintf(heading, sizeof(heading), "%03u°",
                  static_cast<unsigned>(state->heading_deg));
    lv_label_set_text(ui.compass_heading, heading);
    lv_label_set_text(ui.compass_cardinal, cardinal_name(state->heading_deg));
    char speed[20];
    std::snprintf(speed, sizeof(speed), "%u km/h",
                  static_cast<unsigned>(state->speed_kph));
    lv_label_set_text(ui.compass_speed, speed);

    const double heading_rad = state->heading_deg * kPi / 180.0;
    for(int i = 0; i < kCompassTickCount; ++i) {
        const double angle = i * 15.0 * kPi / 180.0 - heading_rad;
        const double inner = px(i % 3 == 0 ? 138.0 : 145.0);
        ui.compass_tick_points[i][0] = {
            static_cast<lv_value_precise_t>(px(180.0) + std::sin(angle) * px(154.0)),
            static_cast<lv_value_precise_t>(px(180.0) - std::cos(angle) * px(154.0)),
        };
        ui.compass_tick_points[i][1] = {
            static_cast<lv_value_precise_t>(px(180.0) + std::sin(angle) * inner),
            static_cast<lv_value_precise_t>(px(180.0) - std::cos(angle) * inner),
        };
        lv_line_set_points_mutable(ui.compass_ticks[i],
                                   ui.compass_tick_points[i], 2);
        lv_obj_set_style_line_color(ui.compass_ticks[i], i == 0 ? kRed : kSoft, 0);
    }

    static const double bearings[4] = {0.0, 90.0, 180.0, 270.0};
    static const char *letters[4] = {"N", "E", "S", "W"};
    for(int i = 0; i < 4; ++i) {
        const double angle = bearings[i] * kPi / 180.0 - heading_rad;
        const int x = static_cast<int>(px(180.0) + std::sin(angle) * px(112.0));
        const int y = static_cast<int>(px(180.0) - std::cos(angle) * px(112.0));
        lv_label_set_text(ui.compass_letters[i], letters[i]);
        lv_obj_set_pos(ui.compass_letters[i], x - px(14), y - px(10));
        lv_obj_set_style_text_color(ui.compass_letters[i], i == 0 ? kRed : kWhite, 0);
    }
}

void update_music_view() {
    lv_label_set_text(ui.music_source, ui.music.connected ? ui.music_source_text : "PHONE NOT CONNECTED");
    lv_label_set_text(ui.music_title, ui.music_title_text);
    lv_label_set_text(ui.music_artist, ui.music_artist_text);
    lv_label_set_text(ui.music_button_labels[1], ui.music.playing ? "II" : ">");
    lv_label_set_text(ui.music_button_labels[3], ui.music.liked ? "SENT" : "LIKE");
    if(ui.music.like_available) {
        lv_obj_remove_flag(ui.music_buttons[3], LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(ui.music_buttons[3], LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_style_border_color(ui.music_disc,
                                  ui.music.playing ? kRed : kGraphite, 0);
}

void music_button_event(lv_event_t *event) {
    const auto command = static_cast<moto_music_command_t>(
        reinterpret_cast<std::intptr_t>(lv_event_get_user_data(event)));
    if(command == MOTO_MUSIC_TOGGLE_PLAYBACK) {
        ui.music.playing = ui.music.playing == 0;
    } else if(command == MOTO_MUSIC_LIKE && ui.music.like_available) {
        ui.music.liked = 1;
    }
    update_music_view();
    if(ui.music_callback != nullptr) ui.music_callback(command, ui.music_callback_context);
}

void create_page_dots() {
    for(int i = 0; i < MOTO_UI_PAGE_COUNT; ++i) {
        ui.page_dots[i] = lv_obj_create(ui.screen);
        lv_obj_remove_style_all(ui.page_dots[i]);
        lv_obj_set_size(ui.page_dots[i], px(5), px(5));
        lv_obj_set_y(ui.page_dots[i], px(MOTO_UI_SQUARE_SCREEN ? 348 : 337));
        lv_obj_set_style_bg_opa(ui.page_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(ui.page_dots[i], px(3), 0);
        lv_obj_clear_flag(ui.page_dots[i], LV_OBJ_FLAG_CLICKABLE);
    }
    ui.page_dots_timer = lv_timer_create(hide_page_dots,
                                         kPageDotsVisibleMs, nullptr);
    update_page_dots();
    reveal_page_dots();
}

void create_navigation_page() {
    lv_obj_t *page = ui.pages[MOTO_UI_PAGE_NAVIGATION];
    ui.nav_map = make_layer(page);
    // The map is the topmost hit target across most of the navigation page.
    // Forward swipe gestures to the page/screen. Local demo activation is not
    // bound to a long press: a glove, mount or palm must never replace active
    // phone guidance with the built-in fixture while riding.
    lv_obj_add_flag(ui.nav_map, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(ui.nav_map, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_height(ui.nav_map, MOTO_UI_NAV_MAP_HEIGHT);

    // Real building footprints are quiet closed outlines beneath both roads
    // and the selected route. No inferred rectangles or decorative fill is
    // generated on-device.
    for(std::uint8_t i = 0;
        i < MOTO_UI_BUILDING_FOOTPRINT_CAPACITY; ++i) {
        ui.nav_buildings[i] = create_map_polyline(ui.nav_map,
                                                  ui.nav_building_lines[i]);
        lv_obj_set_size(ui.nav_buildings[i], MOTO_UI_CANVAS_WIDTH, MOTO_UI_NAV_MAP_HEIGHT);
        lv_obj_set_style_line_width(ui.nav_buildings[i], px(1), 0);
        lv_obj_set_style_line_color(ui.nav_buildings[i], kBuildingGray, 0);
        lv_obj_set_style_line_opa(ui.nav_buildings[i], LV_OPA_COVER, 0);
        lv_obj_set_style_line_rounded(ui.nav_buildings[i], false, 0);
        lv_obj_add_flag(ui.nav_buildings[i], LV_OBJ_FLAG_HIDDEN);
    }

    // Quiet, real street context sits behind the selected route. Each road is
    // its own LVGL line so disconnected streets are never joined by a fake
    // diagonal. The bundled Jinan fixture uses all eight bounded slots; a
    // future online provider must simplify its response to the same limit.
    for(std::uint8_t i = 0; i < MOTO_UI_ROAD_POLYLINE_CAPACITY; ++i) {
        ui.nav_roads[i] = create_map_polyline(ui.nav_map, ui.nav_road_lines[i]);
        lv_obj_set_size(ui.nav_roads[i], MOTO_UI_CANVAS_WIDTH, MOTO_UI_NAV_MAP_HEIGHT);
        lv_obj_set_style_line_width(ui.nav_roads[i], px(3), 0);
        lv_obj_set_style_line_color(ui.nav_roads[i], kRoadGray, 0);
        lv_obj_set_style_line_opa(ui.nav_roads[i], LV_OPA_COVER, 0);
        lv_obj_set_style_line_rounded(ui.nav_roads[i], true, 0);
        lv_obj_add_flag(ui.nav_roads[i], LV_OBJ_FLAG_HIDDEN);
    }

    ui.nav_route_shadow = create_map_polyline(ui.nav_map, ui.nav_route_shadow_line);
    lv_obj_set_size(ui.nav_route_shadow, MOTO_UI_CANVAS_WIDTH, MOTO_UI_NAV_MAP_HEIGHT);
    lv_obj_set_style_line_width(ui.nav_route_shadow, px(13), 0);
    lv_obj_set_style_line_color(ui.nav_route_shadow, kGraphite, 0);
    lv_obj_set_style_line_rounded(ui.nav_route_shadow, true, 0);
    ui.nav_route = create_map_polyline(ui.nav_map, ui.nav_route_line);
    lv_obj_set_size(ui.nav_route, MOTO_UI_CANVAS_WIDTH, MOTO_UI_NAV_MAP_HEIGHT);
    lv_obj_set_style_line_width(ui.nav_route, px(6), 0);
    lv_obj_set_style_line_color(ui.nav_route, kWhite, 0);
    lv_obj_set_style_line_rounded(ui.nav_route, true, 0);
    ui.nav_route_motion_timer = lv_timer_create(route_motion_tick,
                                                 kRouteMotionFrameMs,
                                                 nullptr);

#if MOTO_UI_SQUARE_SCREEN
    // A heading-up location marker is independent of the right-corner roll
    // instrument. Its centre is the shared geographic projection origin.
    ui.nav_marker = lv_image_create(ui.nav_map);
    lv_image_set_src(ui.nav_marker, &ducati_top);
    lv_obj_set_pos(ui.nav_marker,
                   MOTO_UI_NAV_VEHICLE_X - ducati_top.header.w / 2,
                   MOTO_UI_NAV_VEHICLE_Y - ducati_top.header.h / 2);
    lv_obj_remove_flag(ui.nav_marker, LV_OBJ_FLAG_CLICKABLE);
#else
    ui.nav_marker = lv_obj_create(ui.nav_map);
    lv_obj_remove_style_all(ui.nav_marker);
    lv_obj_set_size(ui.nav_marker, px(35), px(37));
    lv_obj_set_pos(ui.nav_marker, px(163), px(177));
    lv_obj_clear_flag(ui.nav_marker, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui.nav_marker, draw_vehicle_marker,
                        LV_EVENT_DRAW_MAIN_END, nullptr);
#endif

    ui.nav_status = make_label(page, &ducati_label_18, kAmber, "");
    lv_obj_set_width(ui.nav_status, px(MOTO_UI_SQUARE_SCREEN ? 108 : 220));
    lv_obj_set_style_text_align(ui.nav_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(ui.nav_status, kBlack, 0);
    lv_obj_set_style_bg_opa(ui.nav_status, LV_OPA_COVER, 0);
    lv_obj_align(ui.nav_status, LV_ALIGN_TOP_MID, 0, px(MOTO_UI_SQUARE_SCREEN ? 26 : 16));

    lv_obj_t *instruction = page;
#if MOTO_UI_SQUARE_SCREEN
    lv_obj_t *model = make_label(page, &ducati_label_18, kWhite, "PANIGALE V2S");
    lv_obj_set_style_bg_color(model, kBlack, 0);
    lv_obj_set_style_bg_opa(model, LV_OPA_COVER, 0);
    lv_obj_align(model, LV_ALIGN_TOP_MID, 0, px(7));
    create_nav_speed_readout(page);
    create_nav_lean_readout(page);
    instruction = lv_obj_create(page);
    lv_obj_remove_style_all(instruction);
    lv_obj_set_pos(instruction, 0, 0);
    lv_obj_set_size(instruction, px(148), px(82));
    lv_obj_set_style_bg_color(instruction, kBlack, 0);
    lv_obj_set_style_bg_opa(instruction, LV_OPA_COVER, 0);
    lv_obj_remove_flag(instruction, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(instruction, LV_OBJ_FLAG_GESTURE_BUBBLE);
#endif

    // The next action is the navigation page's single visual hero. The short
    // road window above is deliberately quieter and contains no fake side
    // streets; this standard symbol must be understood in one glance.
    ui.nav_maneuver = lv_obj_create(instruction);
    lv_obj_remove_style_all(ui.nav_maneuver);
#if MOTO_UI_SQUARE_SCREEN
    lv_obj_set_size(ui.nav_maneuver, px(76), px(73));
    lv_obj_set_pos(ui.nav_maneuver, 0, px(4));
#else
    lv_obj_set_size(ui.nav_maneuver, px(116), px(108));
    lv_obj_set_pos(ui.nav_maneuver, px(29), px(220));
#endif
    lv_obj_clear_flag(ui.nav_maneuver, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui.nav_maneuver, draw_maneuver_icon,
                        LV_EVENT_DRAW_MAIN_END, nullptr);

    ui.nav_distance = make_label(instruction, &ducati_readout_56, kWhite, "300");
    ui.nav_unit = make_label(instruction, &ducati_label_22, kSoft, "m");

    ui.nav_limit = lv_obj_create(page);
    lv_obj_set_size(ui.nav_limit, px(61), px(61));
    // The red ring is the speed-limit sign, independent of the panel shape.
    // Real AMap Web-Service snapshots use zero and remain hidden because that
    // API does not return a road speed limit.
    lv_obj_set_pos(ui.nav_limit, px(MOTO_UI_SQUARE_SCREEN ? 290 : 248),
                                  px(MOTO_UI_SQUARE_SCREEN ? 15 : 238));
    lv_obj_set_style_radius(ui.nav_limit, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ui.nav_limit, kWhite, 0);
    lv_obj_set_style_bg_opa(ui.nav_limit, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(ui.nav_limit, kRed, 0);
    lv_obj_set_style_border_width(ui.nav_limit, px(6), 0);
    lv_obj_set_style_pad_all(ui.nav_limit, 0, 0);
    lv_obj_clear_flag(ui.nav_limit, LV_OBJ_FLAG_CLICKABLE);
    ui.nav_limit_value = make_label(ui.nav_limit,
                                    MOTO_UI_SQUARE_SCREEN ? &ducati_readout_56 : &ducati_readout_32,
                                    kBlack, "70");
    lv_obj_center(ui.nav_limit_value);

#if MOTO_UI_SQUARE_SCREEN
    // Ducati 2.16: use the square panel's full width. A round-screen perimeter
    // arc looks like a circular bezel and crowds the maneuver on this panel.
    ui.nav_progress = lv_bar_create(page);
    lv_obj_remove_style_all(ui.nav_progress);
    lv_obj_set_size(ui.nav_progress, px(324), px(4));
    lv_obj_set_pos(ui.nav_progress, px(18), px(346));
    lv_bar_set_range(ui.nav_progress, 0, 100);
    lv_bar_set_value(ui.nav_progress, 24, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ui.nav_progress, kGraphite, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.nav_progress, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui.nav_progress, kWhite, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(ui.nav_progress, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_clear_flag(ui.nav_progress, LV_OBJ_FLAG_CLICKABLE);
#else
    ui.nav_progress = lv_arc_create(page);
    lv_obj_set_size(ui.nav_progress, px(316), px(316));
    lv_obj_center(ui.nav_progress);
    lv_arc_set_rotation(ui.nav_progress, 48);
    lv_arc_set_bg_angles(ui.nav_progress, 0, 84);
    lv_arc_set_range(ui.nav_progress, 0, 100);
    lv_arc_set_value(ui.nav_progress, 24);
    lv_obj_remove_style(ui.nav_progress, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(ui.nav_progress, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(ui.nav_progress, px(3), LV_PART_MAIN);
    lv_obj_set_style_arc_color(ui.nav_progress, kGraphite, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ui.nav_progress, px(4), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ui.nav_progress, kWhite, LV_PART_INDICATOR);
#endif
    lv_obj_move_to_index(ui.nav_progress, 0);

    // Connection lifecycle overlay. The composition uses one central hero
    // symbol instead of a dashboard of technical status labels.
    ui.nav_lifecycle = make_layer(page);
    lv_obj_set_style_bg_color(ui.nav_lifecycle, kBlack, 0);
    lv_obj_set_style_bg_opa(ui.nav_lifecycle, LV_OPA_COVER, 0);
    lv_obj_add_flag(ui.nav_lifecycle, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(ui.nav_lifecycle, LV_OBJ_FLAG_GESTURE_BUBBLE);

    ui.nav_lifecycle_kicker = make_label(
        ui.nav_lifecycle, &ducati_label_18, kQuiet,
        "MOTO GPS / PHONE LINK");
    lv_obj_set_style_text_letter_space(ui.nav_lifecycle_kicker, px(2), 0);
    lv_obj_align(ui.nav_lifecycle_kicker, LV_ALIGN_TOP_MID, 0, px(39));

    ui.nav_lifecycle_symbol = lv_obj_create(ui.nav_lifecycle);
    lv_obj_remove_style_all(ui.nav_lifecycle_symbol);
    lv_obj_set_size(ui.nav_lifecycle_symbol, px(150), px(150));
    lv_obj_align(ui.nav_lifecycle_symbol, LV_ALIGN_TOP_MID, 0, px(61));
    lv_obj_clear_flag(ui.nav_lifecycle_symbol, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui.nav_lifecycle_symbol, draw_connection_symbol,
                        LV_EVENT_DRAW_MAIN_END, nullptr);

    ui.nav_lifecycle_title = make_label(
        ui.nav_lifecycle, &ducati_readout_32, kWhite, "CONNECT PHONE");
    lv_obj_set_width(ui.nav_lifecycle_title, px(310));
    lv_obj_set_style_text_align(ui.nav_lifecycle_title,
                                LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_letter_space(ui.nav_lifecycle_title, px(2), 0);
    lv_obj_align(ui.nav_lifecycle_title, LV_ALIGN_TOP_MID, 0, px(231));

    ui.nav_lifecycle_subtitle = make_label(
        ui.nav_lifecycle, &moto_font_nav_16, kSoft, "请打开手机应用");
    lv_obj_set_width(ui.nav_lifecycle_subtitle, px(280));
    lv_obj_set_style_text_align(ui.nav_lifecycle_subtitle,
                                LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(ui.nav_lifecycle_subtitle, LV_ALIGN_TOP_MID, 0, px(278));

    ui.nav_lifecycle_timer = lv_timer_create(lifecycle_tick, 40, nullptr);
    ui.nav_success_timer = lv_timer_create(connection_success_timeout,
                                            kConnectionSuccessHoldMs,
                                            nullptr);
    lv_timer_pause(ui.nav_success_timer);
    apply_lifecycle_content(LifecycleVisual::PhoneOffline);
    ui.lifecycle_target = LifecycleVisual::PhoneOffline;
}

void create_speed_page() {
    lv_obj_t *page = ui.pages[MOTO_UI_PAGE_SPEED];
    lv_obj_t *title = make_label(page, &ducati_label_22, kRed, "SPEED");
    lv_obj_set_style_text_letter_space(title, px(2), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, px(78));

    lv_obj_t *model = make_label(page, &ducati_label_18, kWhite, "PANIGALE V2S");
    lv_obj_set_style_text_letter_space(model, 1, 0);
    lv_obj_align(model, LV_ALIGN_TOP_MID, 0, px(7));

    ui.speed_arc = lv_arc_create(page);
    lv_obj_set_size(ui.speed_arc, px(312), px(312));
    lv_obj_center(ui.speed_arc);
    lv_arc_set_rotation(ui.speed_arc, 138);
    lv_arc_set_bg_angles(ui.speed_arc, 0, 264);
    lv_arc_set_range(ui.speed_arc, 0, 160);
    lv_obj_remove_style(ui.speed_arc, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(ui.speed_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(ui.speed_arc, px(3), LV_PART_MAIN);
    lv_obj_set_style_arc_color(ui.speed_arc, kGraphite, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ui.speed_arc, px(5), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ui.speed_arc, kWhite, LV_PART_INDICATOR);

    // The red end of the speed scale echoes the supplied instrument styling.
    // This remains a 0..160 km/h display, with no synthetic engine RPM/gear.
    lv_obj_t *end_band = lv_arc_create(page);
    lv_obj_remove_style_all(end_band);
    lv_obj_set_size(end_band, px(312), px(312));
    lv_obj_center(end_band);
    lv_arc_set_rotation(end_band, 138);
    lv_arc_set_bg_angles(end_band, 231, 264);
    lv_obj_set_style_arc_color(end_band, kRed, LV_PART_MAIN);
    lv_obj_set_style_arc_width(end_band, px(5), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(end_band, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_remove_flag(end_band, LV_OBJ_FLAG_CLICKABLE);

    for(int i = 0; i < kSpeedTickCount; ++i) {
        const double angle = (138.0 + i * (264.0 / (kSpeedTickCount - 1))) * kPi / 180.0;
        const double inner = px(i % 2 == 0 ? 132.0 : 136.0);
        ui.speed_tick_points[i][0] = {
            static_cast<lv_value_precise_t>(px(180.0) + std::cos(angle) * px(142.0)),
            static_cast<lv_value_precise_t>(px(180.0) + std::sin(angle) * px(142.0)),
        };
        ui.speed_tick_points[i][1] = {
            static_cast<lv_value_precise_t>(px(180.0) + std::cos(angle) * inner),
            static_cast<lv_value_precise_t>(px(180.0) + std::sin(angle) * inner),
        };
        ui.speed_ticks[i] = lv_line_create(page);
        lv_obj_set_size(ui.speed_ticks[i], MOTO_UI_CANVAS_WIDTH,
                        MOTO_UI_CANVAS_HEIGHT);
        lv_line_set_points_mutable(ui.speed_ticks[i], ui.speed_tick_points[i], 2);
        lv_obj_set_style_line_width(ui.speed_ticks[i], px(i % 2 == 0 ? 3 : 2), 0);
        lv_obj_set_style_line_color(ui.speed_ticks[i], i >= 14 ? kRed : kSoft, 0);
    }

    for(int value = 0; value <= 160; value += 40) {
        const double angle = (138.0 + value * 264.0 / 160.0) * kPi / 180.0;
        char number[8];
        std::snprintf(number, sizeof(number), "%d", value);
        lv_obj_t *tick_label = make_label(page, &ducati_label_18,
                                           value == 160 ? kRed : kWhite, number);
        lv_obj_set_size(tick_label, px(30), px(16));
        lv_obj_set_style_text_align(tick_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(tick_label,
                        static_cast<int>(std::lround(px(165.0) + std::cos(angle) * px(119.0))),
                        static_cast<int>(std::lround(px(172.0) + std::sin(angle) * px(119.0))));
    }
    ui.speed_value = make_label(page, &ducati_readout_88, kWhite, "72");
    lv_obj_align(ui.speed_value, LV_ALIGN_CENTER, 0, px(-10));
    lv_obj_t *unit = make_label(page, &ducati_label_22, kSoft, "km/h");
    lv_obj_align(unit, LV_ALIGN_CENTER, 0, px(47));
#if MOTO_UI_SQUARE_SCREEN
    ui.speed_lean_value = create_lean_readout(page, ui.speed_lean_graphic, 85, 265, false);
#else
    lv_obj_t *caption = make_label(page, &ducati_label_18, kQuiet, "LIVE SPEED");
    lv_obj_set_style_text_letter_space(caption, px(2), 0);
    lv_obj_align(caption, LV_ALIGN_BOTTOM_MID, 0, px(-49));
#endif
}

void create_compass_page() {
    lv_obj_t *page = ui.pages[MOTO_UI_PAGE_COMPASS];
    for(int i = 0; i < kCompassTickCount; ++i) {
        ui.compass_ticks[i] = lv_line_create(page);
        lv_obj_set_size(ui.compass_ticks[i], MOTO_UI_CANVAS_WIDTH,
                        MOTO_UI_CANVAS_HEIGHT);
        lv_obj_set_style_line_width(ui.compass_ticks[i], px(i % 3 == 0 ? 3 : 2), 0);
        lv_obj_set_style_line_color(ui.compass_ticks[i], kSoft, 0);
    }
    for(int i = 0; i < 4; ++i) {
        ui.compass_letters[i] = make_label(page, &ducati_label_18, kWhite, "N");
        lv_obj_set_size(ui.compass_letters[i], px(28), px(22));
        lv_obj_set_style_text_align(ui.compass_letters[i], LV_TEXT_ALIGN_CENTER, 0);
    }
    lv_obj_t *index = lv_obj_create(page);
    lv_obj_remove_style_all(index);
    lv_obj_set_size(index, px(9), px(9));
    lv_obj_set_pos(index, px(176), px(18));
    lv_obj_set_style_bg_color(index, kRed, 0);
    lv_obj_set_style_bg_opa(index, LV_OPA_COVER, 0);
    lv_obj_set_style_transform_rotation(index, 450, 0);

    ui.compass_heading = make_label(page, &ducati_readout_56, kWhite, "359°");
    lv_obj_align(ui.compass_heading, LV_ALIGN_CENTER, 0, px(-37));
    ui.compass_cardinal = make_label(page, &ducati_label_22, kRed, "N");
    lv_obj_align(ui.compass_cardinal, LV_ALIGN_CENTER, 0, px(10));
    ui.compass_speed = make_label(page, &ducati_label_22, kWhite, "72 km/h");
    lv_obj_set_style_text_letter_space(ui.compass_speed, px(1), 0);
    lv_obj_align(ui.compass_speed, LV_ALIGN_CENTER, 0, px(57));
}

void create_music_page() {
    lv_obj_t *page = ui.pages[MOTO_UI_PAGE_MUSIC];
    ui.music_source = make_label(page, &ducati_label_18, kRed, "APPLE MUSIC");
    lv_obj_set_style_text_letter_space(ui.music_source, px(2), 0);
    lv_obj_align(ui.music_source, LV_ALIGN_TOP_MID, 0, px(37));

    ui.music_disc = lv_obj_create(page);
    lv_obj_set_size(ui.music_disc, px(104), px(104));
    lv_obj_align(ui.music_disc, LV_ALIGN_TOP_MID, 0, px(71));
    lv_obj_set_style_radius(ui.music_disc, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ui.music_disc, kGraphite, 0);
    lv_obj_set_style_bg_opa(ui.music_disc, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ui.music_disc, px(5), 0);
    lv_obj_set_style_border_color(ui.music_disc, kRed, 0);
    lv_obj_set_style_pad_all(ui.music_disc, 0, 0);
    lv_obj_t *core = lv_obj_create(ui.music_disc);
    lv_obj_set_size(core, px(30), px(30));
    lv_obj_center(core);
    lv_obj_set_style_radius(core, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(core, kBlack, 0);
    lv_obj_set_style_bg_opa(core, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(core, 0, 0);

    ui.music_title = make_label(page, &ducati_label_22, kWhite, "NIGHT RIDE");
    lv_obj_set_width(ui.music_title, px(260));
    lv_obj_set_style_text_align(ui.music_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(ui.music_title, LV_LABEL_LONG_DOT);
    lv_obj_align(ui.music_title, LV_ALIGN_TOP_MID, 0, px(190));
    ui.music_artist = make_label(page, &ducati_label_18, kQuiet, "PHONE NOW PLAYING");
    lv_obj_set_width(ui.music_artist, px(260));
    lv_obj_set_style_text_align(ui.music_artist, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(ui.music_artist, LV_LABEL_LONG_DOT);
    lv_obj_align(ui.music_artist, LV_ALIGN_TOP_MID, 0, px(216));

    static const char *labels[4] = {"<<", ">", ">>", "LIKE"};
    static const int positions[4][3] = {
        {72, 254, 54}, {153, 247, 62}, {234, 254, 54}, {147, 312, 66},
    };
    for(int i = 0; i < 4; ++i) {
        ui.music_buttons[i] = lv_button_create(page);
        lv_obj_set_size(ui.music_buttons[i], px(positions[i][2]),
                        px(i == 3 ? 28 : positions[i][2]));
        lv_obj_set_pos(ui.music_buttons[i], px(positions[i][0]),
                       px(positions[i][1]));
        lv_obj_set_style_radius(ui.music_buttons[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(ui.music_buttons[i], i == 1 ? kWhite : kGraphite, 0);
        lv_obj_set_style_bg_opa(ui.music_buttons[i], LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_width(ui.music_buttons[i], 0, 0);
        lv_obj_set_style_border_width(ui.music_buttons[i], 0, 0);
        // Pin the pressed appearance to the idle appearance. The LVGL default
        // theme darkens and grows pressed buttons (recolor 35% + 3 px grow);
        // on the ESP32 PPA render path that pre-composited recolor painted as
        // a hard-edged yellow-green block over the round button. Identical
        // pressed styles remove the visual state difference by construction.
        lv_obj_set_style_bg_color(ui.music_buttons[i], i == 1 ? kWhite : kGraphite,
                                  LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(ui.music_buttons[i], LV_OPA_COVER,
                                LV_STATE_PRESSED);
        lv_obj_set_style_radius(ui.music_buttons[i], LV_RADIUS_CIRCLE,
                                LV_STATE_PRESSED);
        lv_obj_set_style_shadow_width(ui.music_buttons[i], 0, LV_STATE_PRESSED);
        lv_obj_set_style_border_width(ui.music_buttons[i], 0, LV_STATE_PRESSED);
        lv_obj_set_style_recolor_opa(ui.music_buttons[i], LV_OPA_TRANSP,
                                     LV_STATE_PRESSED);
        lv_obj_set_style_transform_width(ui.music_buttons[i], 0,
                                         LV_STATE_PRESSED);
        lv_obj_set_style_transform_height(ui.music_buttons[i], 0,
                                          LV_STATE_PRESSED);
        lv_obj_add_flag(ui.music_buttons[i], LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_add_event_cb(
            ui.music_buttons[i], music_button_event, LV_EVENT_CLICKED,
            reinterpret_cast<void *>(static_cast<std::intptr_t>(i)));
        ui.music_button_labels[i] = make_label(
            ui.music_buttons[i], i == 3 ? &ducati_label_18 : &ducati_label_22,
            i == 1 ? kBlack : kWhite, labels[i]);
        lv_obj_center(ui.music_button_labels[i]);
    }
}

void set_boot_content_opacity(void *object, int32_t opacity) {
    lv_obj_set_style_opa(static_cast<lv_obj_t *>(object), opacity, 0);
}

}  // namespace

extern "C" void moto_nav_ui_show_boot_screen(void) {
    /* This entry point is also used while the full UI is live. Delete every
       timer that retains widget pointers before reset_ui_state() and
       lv_obj_clean() invalidate the tree. */
    if(ui.page_dots_timer != nullptr) {
        lv_timer_delete(ui.page_dots_timer);
        ui.page_dots_timer = nullptr;
    }
    if(ui.nav_route_motion_timer != nullptr) {
        lv_timer_delete(ui.nav_route_motion_timer);
        ui.nav_route_motion_timer = nullptr;
    }
    if(ui.nav_lifecycle_timer != nullptr) {
        lv_timer_delete(ui.nav_lifecycle_timer);
        ui.nav_lifecycle_timer = nullptr;
    }
    if(ui.nav_success_timer != nullptr) {
        lv_timer_delete(ui.nav_success_timer);
        ui.nav_success_timer = nullptr;
    }
    reset_ui_state();
    ui.screen = lv_screen_active();
    lv_obj_clean(ui.screen);
    lv_obj_remove_flag(ui.screen, LV_OBJ_FLAG_SCROLLABLE);
    const lv_color_t boot_black = LV_COLOR_MAKE(0x00, 0x00, 0x00);
    const lv_color_t boot_white = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF);
    lv_obj_set_style_bg_color(ui.screen, boot_black, 0);
    lv_obj_set_style_bg_opa(ui.screen, LV_OPA_COVER, 0);

    // Keep the AMOLED background truly black and animate only the white mark.
    // A single opacity animation with a delayed reverse is cheaper than
    // per-letter animation and fits inside the existing 1.25 s boot cadence.
    lv_obj_t *content = make_layer(ui.screen);
    lv_obj_set_style_opa(content, LV_OPA_TRANSP, 0);

    lv_obj_t *moto = make_label(content, &lv_font_montserrat_48,
                                boot_white, "MOTO");
    lv_obj_set_style_text_letter_space(moto, px(4), 0);
    lv_obj_set_style_text_outline_stroke_color(moto, boot_white, 0);
    lv_obj_set_style_text_outline_stroke_width(moto, px(2), 0);
    lv_obj_set_style_text_outline_stroke_opa(moto, LV_OPA_COVER, 0);
    lv_obj_set_style_transform_scale_x(moto, 340, 0);
    lv_obj_set_style_transform_scale_y(moto, 340, 0);
    lv_obj_align(moto, LV_ALIGN_CENTER, 0, px(-37));

    lv_obj_t *gps = make_label(content, &lv_font_montserrat_48,
                               boot_white, "GPS");
    lv_obj_set_style_text_letter_space(gps, px(10), 0);
    lv_obj_set_style_text_outline_stroke_color(gps, boot_white, 0);
    lv_obj_set_style_text_outline_stroke_width(gps, px(2), 0);
    lv_obj_set_style_text_outline_stroke_opa(gps, LV_OPA_COVER, 0);
    lv_obj_set_style_transform_scale_x(gps, 340, 0);
    lv_obj_set_style_transform_scale_y(gps, 340, 0);
    lv_obj_align(gps, LV_ALIGN_CENTER, 0, px(35));

    // One deliberate signature: a road-like diagonal cut through two solid
    // white velocity bars. No grey, colour or decorative loading spinner.
    for(int i = 0; i < 2; ++i) {
        lv_obj_t *bar = lv_obj_create(content);
        lv_obj_remove_style_all(bar);
        lv_obj_set_size(bar, px(i == 0 ? 56 : 38), px(5));
        lv_obj_set_pos(bar, px(i == 0 ? 73 : 250), px(i == 0 ? 259 : 94));
        lv_obj_set_style_bg_color(bar, boot_white, 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_set_style_transform_rotation(bar, -120, 0);
    }

    lv_anim_t fade;
    lv_anim_init(&fade);
    lv_anim_set_var(&fade, content);
    lv_anim_set_exec_cb(&fade, set_boot_content_opacity);
    lv_anim_set_values(&fade, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_delay(&fade, 30);
    lv_anim_set_duration(&fade, 230);
    lv_anim_set_reverse_delay(&fade, 650);
    lv_anim_set_reverse_duration(&fade, 260);
    lv_anim_set_path_cb(&fade, lv_anim_path_ease_in_out);
    lv_anim_start(&fade);
}

extern "C" void moto_nav_ui_show_power_off_screen(void) {
    if(ui.screen == nullptr) return;
    if(ui.page_dots_timer != nullptr) {
        lv_timer_delete(ui.page_dots_timer);
        ui.page_dots_timer = nullptr;
    }
    if(ui.nav_route_motion_timer != nullptr) {
        lv_timer_delete(ui.nav_route_motion_timer);
        ui.nav_route_motion_timer = nullptr;
    }
    if(ui.nav_lifecycle_timer != nullptr) {
        lv_timer_delete(ui.nav_lifecycle_timer);
        ui.nav_lifecycle_timer = nullptr;
    }
    if(ui.nav_success_timer != nullptr) {
        lv_timer_delete(ui.nav_success_timer);
        ui.nav_success_timer = nullptr;
    }

    lv_obj_clean(ui.screen);
    lv_obj_set_style_bg_color(ui.screen, LV_COLOR_MAKE(0x00, 0x00, 0x00), 0);
    lv_obj_set_style_bg_opa(ui.screen, LV_OPA_COVER, 0);

    lv_obj_t *brand = make_label(ui.screen, &lv_font_montserrat_48,
                                 LV_COLOR_MAKE(0xFF, 0xFF, 0xFF), "MOTO");
    lv_obj_set_style_text_letter_space(brand, px(4), 0);
    lv_obj_set_style_text_outline_stroke_color(
        brand, LV_COLOR_MAKE(0xFF, 0xFF, 0xFF), 0);
    lv_obj_set_style_text_outline_stroke_width(brand, px(2), 0);
    lv_obj_set_style_text_outline_stroke_opa(brand, LV_OPA_COVER, 0);
    lv_obj_align(brand, LV_ALIGN_CENTER, 0, px(-18));

    lv_obj_t *status = make_label(ui.screen, &lv_font_montserrat_16,
                                  LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),
                                  "POWER OFF");
    lv_obj_set_style_text_letter_space(status, px(4), 0);
    lv_obj_align(status, LV_ALIGN_CENTER, 0, px(35));
}

extern "C" void moto_nav_ui_create(void) {
    // Recreating the UI must cancel callbacks before resetting their retained
    // widget pointers. Otherwise an earlier page-dot timer can outlive the tree
    // and later fire while a boot screen has replaced all navigation widgets.
    lv_timer_t *old_timers[] = {ui.page_dots_timer, ui.nav_route_motion_timer,
                                 ui.nav_lifecycle_timer, ui.nav_success_timer};
    for(lv_timer_t *timer : old_timers) {
        if(timer != nullptr) lv_timer_delete(timer);
    }
    reset_ui_state();
    ui.screen = lv_screen_active();
    lv_obj_clean(ui.screen);
    lv_obj_remove_flag(ui.screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui.screen, kBlack, 0);
    lv_obj_set_style_bg_opa(ui.screen, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(ui.screen, gesture_event, LV_EVENT_GESTURE, nullptr);
    for(int i = 0; i < MOTO_UI_PAGE_COUNT; ++i) {
        ui.pages[i] = make_layer(ui.screen);
        lv_obj_add_flag(ui.pages[i], LV_OBJ_FLAG_GESTURE_BUBBLE);
    }
    create_navigation_page();
    create_speed_page();
    create_compass_page();
    create_music_page();
    create_page_dots();
    // Press events are delivered to the topmost object under the finger, not
    // necessarily to its page. Register once across the finished tree so any
    // touch reliably wakes the dots, including the map and music controls.
    install_interaction_wake(ui.screen);

    moto_music_state_t initial_music = {
        "APPLE MUSIC", "NIGHT RIDE", "PHONE NOW PLAYING", 1, 1, 0, 0,
    };
    moto_nav_ui_set_music_state(&initial_music);

    moto_ui_state_t initial{};
    initial.mode = MOTO_UI_ACQUIRING_FIX;
    initial.page = MOTO_UI_PAGE_NAVIGATION;
    initial.maneuver = MOTO_MANEUVER_STRAIGHT;
    initial.traffic = MOTO_TRAFFIC_UNKNOWN;
    moto_nav_ui_set_state(&initial);
    show_page(MOTO_UI_PAGE_NAVIGATION, true);
}

extern "C" void moto_nav_ui_set_state(const moto_ui_state_t *state) {
    if(state == nullptr || ui.screen == nullptr) return;
    show_page(state->page);
    // Hidden pages do not need to be invalidated. Page changes immediately
    // apply a fresh snapshot through PhoneNavBridge, so this keeps every page
    // correct while avoiding three full page redraws per navigation update.
    switch(ui.page) {
        case MOTO_UI_PAGE_NAVIGATION: update_navigation(state); break;
        case MOTO_UI_PAGE_SPEED: update_speedometer(state); break;
        case MOTO_UI_PAGE_COMPASS: update_compass(state); break;
        case MOTO_UI_PAGE_MUSIC:
        case MOTO_UI_PAGE_COUNT: break;
    }
}

extern "C" void moto_nav_ui_set_lean_state(const moto_ui_state_t *state) {
    if(state == nullptr || ui.screen == nullptr || state->page != ui.page) return;
    if(ui.page == MOTO_UI_PAGE_NAVIGATION)
        update_lean(ui.nav_lean_value, ui.nav_lean_graphic, state);
    else if(ui.page == MOTO_UI_PAGE_SPEED)
        update_lean(ui.speed_lean_value, ui.speed_lean_graphic, state);
}

extern "C" void moto_nav_ui_set_motion_state(const moto_ui_state_t *state) {
    if(state == nullptr || ui.screen == nullptr || state->page != ui.page) return;
    moto_nav_ui_set_lean_state(state);
    // QMI8658 samples arrive at high frequency. Only the route polyline or
    // compass rose actually changes with yaw; labels, arcs and hidden pages
    // remain untouched so LVGL submits a small, bounded dirty region.
    if(ui.page == MOTO_UI_PAGE_NAVIGATION) {
#if MOTO_UI_SQUARE_SCREEN
        update_nav_speed_readout(state);
#endif
        update_building_geometry(state);
        update_road_geometry(state);
        update_route_geometry(state);
    } else if(ui.page == MOTO_UI_PAGE_COMPASS) {
        update_compass(state);
    }
}

extern "C" void moto_nav_ui_set_phone_connection(
    moto_ui_phone_connection_t connection) {
    if(ui.screen == nullptr || connection < MOTO_UI_PHONE_OFFLINE ||
       connection > MOTO_UI_PHONE_ONLINE) {
        return;
    }
    if(ui.phone_connection == connection) return;
    const bool became_online =
        connection == MOTO_UI_PHONE_ONLINE &&
        ui.phone_connection != MOTO_UI_PHONE_ONLINE;
    ui.phone_connection = connection;
    if(became_online) {
        ui.lifecycle_success_active = true;
        if(ui.nav_success_timer != nullptr) {
            lv_timer_set_period(ui.nav_success_timer,
                                kConnectionSuccessHoldMs);
            lv_timer_reset(ui.nav_success_timer);
            lv_timer_resume(ui.nav_success_timer);
        }
    } else if(connection != MOTO_UI_PHONE_ONLINE) {
        ui.lifecycle_success_active = false;
        if(ui.nav_success_timer != nullptr) {
            lv_timer_pause(ui.nav_success_timer);
        }
    }
    refresh_lifecycle();
}

extern "C" void moto_nav_ui_set_lean_calibration_callback(
    moto_lean_calibration_callback_t callback, void *context) {
    ui.lean_calibration_callback = callback;
    ui.lean_calibration_context = context;
}

extern "C" void moto_nav_ui_set_reduce_motion(uint8_t reduce_motion) {
    ui.reduce_motion = reduce_motion != 0;
    if(ui.nav_lifecycle_timer != nullptr) {
        if(ui.reduce_motion) {
            lv_timer_pause(ui.nav_lifecycle_timer);
        } else {
            lv_timer_resume(ui.nav_lifecycle_timer);
        }
    }
    refresh_lifecycle();
}

extern "C" void moto_nav_ui_set_page(moto_ui_page_t page) {
    if(ui.screen != nullptr) show_page(page, true);
}

extern "C" moto_ui_page_t moto_nav_ui_get_page(void) {
    return ui.page;
}

extern "C" void moto_nav_ui_set_page_change_callback(
    moto_page_change_callback_t callback, void *context) {
    ui.page_callback = callback;
    ui.page_callback_context = context;
}

extern "C" void moto_nav_ui_set_music_state(const moto_music_state_t *state) {
    if(state == nullptr || ui.screen == nullptr) return;
    ui.music = *state;
    copy_text(ui.music_source_text, sizeof(ui.music_source_text),
              state->source_name, "PHONE MEDIA");
    copy_text(ui.music_title_text, sizeof(ui.music_title_text),
              state->track_title, "--");
    copy_text(ui.music_artist_text, sizeof(ui.music_artist_text),
              state->artist_name, "--");
    ui.music.source_name = ui.music_source_text;
    ui.music.track_title = ui.music_title_text;
    ui.music.artist_name = ui.music_artist_text;
    update_music_view();
}

extern "C" void moto_nav_ui_set_music_page_enabled(uint8_t enabled) {
    if(ui.screen == nullptr) return;
    ui.music_page_enabled = enabled != 0;
    if(!ui.music_page_enabled && ui.page == MOTO_UI_PAGE_MUSIC) {
        show_page(MOTO_UI_PAGE_NAVIGATION, true);
    }
    update_page_dots();
}

extern "C" void moto_nav_ui_set_music_command_callback(
    moto_music_command_callback_t callback, void *context) {
    ui.music_callback = callback;
    ui.music_callback_context = context;
}

extern "C" void moto_nav_ui_set_demo_active(uint8_t enabled) {
    ui.demo_active = enabled != 0;
    refresh_lifecycle();
}

extern "C" void moto_nav_ui_set_demo_change_callback(
    moto_demo_change_callback_t callback, void *context) {
    ui.demo_callback = callback;
    ui.demo_callback_context = context;
}
