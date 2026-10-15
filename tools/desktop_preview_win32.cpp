// Windows viewer for the same LVGL boot and UI code used by the firmware.
// No Bluetooth, sensor, PMIC or network service is started by this program.
#include <windows.h>

#include "ducati_boot_animation.h"
#include "moto_nav_ui.h"
#include "lvgl.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <fstream>
#include <string>
#include <vector>

namespace {
constexpr int kWidth = MOTO_UI_CANVAS_WIDTH;
constexpr int kHeight = MOTO_UI_CANVAS_HEIGHT;
constexpr int kToolbar = 52;
constexpr int kStatus = 32;
constexpr int kReplay = 101;
constexpr int kPause = 102;
constexpr int kDemo = 103;
std::vector<std::uint16_t> pixels(kWidth * kHeight);
std::vector<std::uint16_t> draw_buffer(kWidth * 40);
std::vector<std::uint32_t> extra_pool(2 * 1024 * 1024 / 4);
lv_display_t* display = nullptr;
HWND pause_button = nullptr;
HWND status_label = nullptr;
bool paused = false;
bool navigation = false;
std::uint32_t boot_elapsed = 0;
std::uint32_t demo_elapsed = 0;
ULONGLONG previous_tick = 0;
moto_ui_state_t state{};

int scale(int value) { return (value * kWidth + 240) / 480; }
moto_ui_point_t point(int x, int y) {
    return {static_cast<std::int16_t>(scale(x)), static_cast<std::int16_t>(scale(y))};
}

void flush(lv_display_t* target, const lv_area_t* area, std::uint8_t* source) {
    const int width = area->x2 - area->x1 + 1;
    for (int y = area->y1; y <= area->y2; ++y) {
        std::memcpy(pixels.data() + y * kWidth + area->x1,
                    source + (y - area->y1) * width * 2, width * 2);
    }
    lv_display_flush_ready(target);
}

void replay() {
    // This upstream entry point cancels its navigation timers. Replace its
    // temporary tree immediately, before any refresh can show that wordmark.
    moto_nav_ui_show_boot_screen();
    ducati_boot_animation_show();
    navigation = false;
    paused = false;
    boot_elapsed = 0;
    demo_elapsed = 0;
    lv_refr_now(display);
}

bool initialize() {
    lv_init();
    if (!lv_mem_add_pool(extra_pool.data(), extra_pool.size() * sizeof(std::uint32_t))) return false;
    display = lv_display_create(kWidth, kHeight);
    if (!display) return false;
    lv_display_set_flush_cb(display, flush);
    lv_display_set_buffers(display, draw_buffer.data(), nullptr,
                           draw_buffer.size() * sizeof(std::uint16_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
    replay();
    return true;
}

void start_demo() {
    if (navigation) return;
    moto_nav_ui_create();
    moto_nav_ui_set_music_page_enabled(0);  // Match the 2.16 firmware.
    moto_nav_ui_set_demo_active(1);
    moto_nav_ui_set_phone_connection(MOTO_UI_PHONE_ONLINE);  // Simulated link only.
    state = moto_ui_state_t{};
    state.page = MOTO_UI_PAGE_NAVIGATION;
    state.mode = MOTO_UI_NAVIGATING;
    state.maneuver = MOTO_MANEUVER_RIGHT;
    state.traffic = MOTO_TRAFFIC_CLEAR;
    state.distance_to_maneuver_m = 180;
    state.remaining_distance_m = 2500;
    state.remaining_time_s = 300;
    state.route_progress_percent = 25;
    state.gps_accuracy_m = 5;
    state.online = 1;
    state.has_destination = 1;
    state.route_identity = 1;
    state.route_generation = 1;
    state.map_scene_revision = 1;
    state.speed_limit_kph = 50;
    state.lean_valid = 1;
    state.road_name = "DEMO ROAD";
    state.next_road_name = "NEXT TURN";
    state.route_point_count = 4;
    state.route_points[0] = {MOTO_UI_NAV_VEHICLE_X, MOTO_UI_NAV_VEHICLE_Y};
    state.route_points[1] = point(240, 195);
    state.route_points[2] = point(320, 125);
    state.route_points[3] = point(320, 25);
    state.road_point_count = 6;
    state.road_points[0] = point(20, 195);
    state.road_points[1] = point(460, 195);
    state.road_points[2] = point(140, 100);
    state.road_points[3] = point(140, 315);
    state.road_points[4] = point(340, 100);
    state.road_points[5] = point(340, 315);
    state.road_polyline_count = 3;
    state.road_polylines[0] = {0, 2, 1};
    state.road_polylines[1] = {2, 2, 2};
    state.road_polylines[2] = {4, 2, 1};
    state.building_point_count = 4;
    state.building_points[0] = point(45, 120);
    state.building_points[1] = point(100, 120);
    state.building_points[2] = point(100, 180);
    state.building_points[3] = point(45, 180);
    state.building_footprint_count = 1;
    state.building_footprints[0] = {0, 4, 1};
    navigation = true;
    paused = false;
    demo_elapsed = 0;
    moto_nav_ui_set_state(&state);
}

void advance(std::uint32_t milliseconds) {
    if (paused) return;
    lv_tick_inc(milliseconds);
    lv_timer_handler();
    if (!navigation) {
        boot_elapsed += milliseconds;
        if (boot_elapsed >= DUCATI_BOOT_ANIMATION_DURATION_MS + 25) start_demo();
    } else {
        demo_elapsed += milliseconds;
        state.speed_kph = static_cast<std::uint16_t>(58.0 + 8.0 * std::sin(demo_elapsed / 1000.0));
        state.heading_deg = static_cast<std::uint16_t>((demo_elapsed / 40) % 360);
        state.lean_deg = static_cast<float>(32.0 * std::sin(demo_elapsed / 1800.0));
        state.distance_to_maneuver_m = 180 - (demo_elapsed / 200) % 160;
        moto_nav_ui_set_state(&state);
    }
}

void page(int index) {
    start_demo();
    state.page = static_cast<moto_ui_page_t>(index);
    moto_nav_ui_set_state(&state);
    lv_refr_now(display);
}

void update_controls() {
    SetWindowTextW(pause_button, paused ? L"继续 Space" : L"暂停 Space");
    const wchar_t* text = navigation
        ? L"模拟数据 · 1 导航 / 2 速度 / 3 罗盘 · 左右方向键切页"
        : L"真实 LVGL 启动动画 · R 重播 · N 导航演示";
    SetWindowTextW(status_label, text);
}

bool key(WPARAM value) {
    switch (value) {
        case 'R': replay(); break;
        case 'N': start_demo(); break;
        case VK_SPACE: paused = !paused; break;
        case '1': page(0); break;
        case '2': page(1); break;
        case '3': page(2); break;
        case VK_LEFT: page((navigation ? static_cast<int>(state.page) + 2 : 0) % 3); break;
        case VK_RIGHT: page((navigation ? static_cast<int>(state.page) + 1 : 0) % 3); break;
        default: return false;
    }
    update_controls();
    return true;
}

LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_CREATE: {
            auto button = [window](const wchar_t* label, int id, int x, int width) {
                HWND handle = CreateWindowExW(0, L"BUTTON", label, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                               x, 10, width, 30, window,
                                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
                SendMessageW(handle, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
                return handle;
            };
            button(L"重播 R", kReplay, 12, 96);
            pause_button = button(L"暂停 Space", kPause, 116, 125);
            button(L"导航演示 N", kDemo, 249, 135);
            status_label = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER,
                                           0, 0, 0, 0, window, nullptr, nullptr, nullptr);
            SendMessageW(status_label, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
            update_controls();
            previous_tick = GetTickCount64();
            SetTimer(window, 1, 25, nullptr);
            return 0;
        }
        case WM_SIZE: {
            const int width = LOWORD(lparam);
            const int height = HIWORD(lparam);
            MoveWindow(status_label, 8, height - kStatus + 5, std::max(0, width - 16), 24, TRUE);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        case WM_GETMINMAXINFO: {
            auto* limits = reinterpret_cast<MINMAXINFO*>(lparam);
            limits->ptMinTrackSize = {480, 590};
            return 0;
        }
        case WM_TIMER: {
            const ULONGLONG now = GetTickCount64();
            const auto delta = static_cast<std::uint32_t>(std::min<ULONGLONG>(now - previous_tick, 1000));
            previous_tick = now;
            const bool was_navigation = navigation;
            advance(delta);
            if (navigation != was_navigation) update_controls();
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        case WM_COMMAND:
            switch (LOWORD(wparam)) {
                case kReplay: key('R'); break;
                case kPause: key(VK_SPACE); break;
                case kDemo: key('N'); break;
                default: break;
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC context = BeginPaint(window, &paint);
            RECT client{};
            GetClientRect(window, &client);
            RECT stage{0, kToolbar, client.right, client.bottom - kStatus};
            FillRect(context, &stage, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
            const int side = std::max(1L, std::min(client.right - 24, stage.bottom - stage.top - 16));
            const int x = (client.right - side) / 2;
            const int y = stage.top + (stage.bottom - stage.top - side) / 2;
            struct { BITMAPINFOHEADER header; DWORD masks[3]; } bitmap{};
            bitmap.header.biSize = sizeof(BITMAPINFOHEADER);
            bitmap.header.biWidth = kWidth;
            bitmap.header.biHeight = -kHeight;  // LVGL flush rows are top-down.
            bitmap.header.biPlanes = 1;
            bitmap.header.biBitCount = 16;
            bitmap.header.biCompression = BI_BITFIELDS;
            bitmap.masks[0] = 0xF800;
            bitmap.masks[1] = 0x07E0;
            bitmap.masks[2] = 0x001F;
            SetStretchBltMode(context, COLORONCOLOR);
            StretchDIBits(context, x, y, side, side, 0, 0, kWidth, kHeight, pixels.data(),
                           reinterpret_cast<const BITMAPINFO*>(&bitmap), DIB_RGB_COLORS, SRCCOPY);
            EndPaint(window, &paint);
            return 0;
        }
        case WM_DESTROY:
            KillTimer(window, 1);
            PostQuitMessage(0);
            return 0;
        default: return DefWindowProcW(window, message, wparam, lparam);
    }
}

bool black() {
    return std::all_of(pixels.begin(), pixels.end(), [](std::uint16_t pixel) { return pixel == 0; });
}

bool save_frame(const char* name) {
    // Let deferred layout/label refresh finish between rapid headless page
    // switches; do not advance the synthetic riding data while taking a still.
    lv_tick_inc(25);
    lv_timer_handler();
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(display);
    std::ofstream output(name, std::ios::binary);
    output << "P6\n" << kWidth << ' ' << kHeight << "\n255\n";
    for (std::uint16_t color : pixels) {
        const char rgb[] = {
            static_cast<char>((((color >> 11) & 31) * 255 + 15) / 31),
            static_cast<char>((((color >> 5) & 63) * 255 + 31) / 63),
            static_cast<char>(((color & 31) * 255 + 15) / 31),
        };
        output.write(rgb, sizeof(rgb));
    }
    return output.good();
}

int capture_pages() {
    start_demo();
    moto_nav_ui_set_reduce_motion(1);  // Capture settled UI, without transition overlays.
    advance(25);
    state.lean_deg = 23.0F;
    const char* names[] = {"navigation.ppm", "speed.ppm", "compass.ppm"};
    for (int index = 0; index < 3; ++index) {
        page(index);
        if (!save_frame(names[index])) return 20;
    }
    struct LeanCase { const char* name; float angle; bool valid; };
    const LeanCase cases[] = {{"right-30", 30, true}, {"left-30", -30, true},
        {"upright", 0, true}, {"unavailable", 0, false}, {"right-85", 85, true}};
    for (const auto& test : cases) {
        state.lean_deg = test.angle;
        state.lean_valid = test.valid;
        for (int index = 0; index < 2; ++index) {
            page(index);
            const std::string name = std::string(index ? "speed-" : "navigation-") + test.name + ".ppm";
            if (!save_frame(name.c_str())) return 20;
        }
    }
    state.lean_deg = 0.0F;
    state.lean_valid = 1;
    state.speed_kph = 58;
    page(0);
    if (!save_frame("navigation-approved.ppm")) return 20;
    state.speed_kph = 160;
    page(0);
    if (!save_frame("navigation-high-speed.ppm")) return 20;
    state.speed_limit_kph = 0;
    page(0);
    if (!save_frame("navigation-no-limit.ppm")) return 20;
    state.speed_kph = 58;
    state.speed_limit_kph = 50;
    state.distance_to_maneuver_m = 12300;
    page(0);
    if (!save_frame("navigation-km.ppm")) return 20;
    return 0;
}

int self_test() {
    if (!black()) return 2;
    for (int i = 0; i < 76; ++i) advance(25);
    lv_refr_now(display);
    if (navigation || black()) return 3;
    for (int i = 0; i < 100; ++i) advance(25);
    lv_refr_now(display);
    if (!navigation || black()) return 4;
    for (int index = 0; index < 3; ++index) {
        page(index);
        advance(100);
        lv_refr_now(display);
        if (black() || moto_nav_ui_get_page() != index) return 5;
    }
    paused = true;
    const auto paused_frame = pixels;
    advance(1000);
    if (pixels != paused_frame) return 6;
    replay();  // Cancels navigation timers before replacing their widgets.
    if (!black()) return 7;
    for (int i = 0; i < 180; ++i) advance(25);
    lv_refr_now(display);
    return navigation && !black() ? 0 : 8;
}
}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR arguments, int show) {
    if (!initialize()) return 1;
    if (std::wcsstr(arguments, L"--capture")) {
        const int result = capture_pages();
        lv_display_delete(display);
        lv_deinit();
        return result;
    }
    if (std::wcsstr(arguments, L"--self-test")) {
        const int result = self_test();
        lv_display_delete(display);
        lv_deinit();
        return result;
    }
    SetProcessDPIAware();
    WNDCLASSW window_class{};
    window_class.lpfnWndProc = window_procedure;
    window_class.hInstance = instance;
    window_class.lpszClassName = L"DucatiLvglPreview";
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    if (!RegisterClassW(&window_class)) return 10;
    RECT rectangle{0, 0, kWidth + 32, kHeight + kToolbar + kStatus + 16};
    AdjustWindowRect(&rectangle, WS_OVERLAPPEDWINDOW, FALSE);
    HWND window = CreateWindowExW(0, window_class.lpszClassName,
        L"Ducati PANIGALE V2S · 本机 LVGL 模拟器", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, rectangle.right - rectangle.left, rectangle.bottom - rectangle.top,
        nullptr, nullptr, instance, nullptr);
    if (!window) return 11;
    ShowWindow(window, show);
    MSG message{};
    BOOL received = 0;
    while ((received = GetMessageW(&message, nullptr, 0, 0)) > 0) {
        if (message.message == WM_KEYDOWN && key(message.wParam)) {
            InvalidateRect(window, nullptr, FALSE);
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    lv_display_delete(display);
    lv_deinit();
    return received < 0 ? 12 : static_cast<int>(message.wParam);
}
