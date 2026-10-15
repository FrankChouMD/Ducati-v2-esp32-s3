cmake_minimum_required(VERSION 3.20)
project(ducati_v2_host_checks LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(DUCATI_CANVAS_SIZE "480" CACHE STRING "Panel size for native UI checks")
if(NOT DUCATI_CANVAS_SIZE MATCHES "^(466|480)$")
    message(FATAL_ERROR "DUCATI_CANVAS_SIZE must be 466 or 480")
endif()
add_compile_definitions(
    MOTO_UI_CANVAS_WIDTH=${DUCATI_CANVAS_SIZE}
    MOTO_UI_CANVAS_HEIGHT=${DUCATI_CANVAS_SIZE}
)
if(MSVC)
    add_compile_options(/utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

set(MOTO_UPSTREAM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/moto-gps-waveshare")
set(LV_BUILD_CONF_PATH "${MOTO_UPSTREAM_ROOT}/config/lv_conf.h" CACHE PATH "Shared LVGL configuration" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "No LVGL demos" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "No LVGL examples" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "No vector engine in native navigation checks" FORCE)

# Register LVGL before the upstream UI target, whose standalone entry point
# assumes a different repository root.
add_subdirectory("${MOTO_UPSTREAM_ROOT}/third_party/lvgl" "lvgl")
add_subdirectory("${MOTO_UPSTREAM_ROOT}/shared/coordinates" "coordinates")
add_subdirectory("${MOTO_UPSTREAM_ROOT}/shared/nav_core" "nav_core")
add_subdirectory("${MOTO_UPSTREAM_ROOT}/shared/nav_app" "nav_app")
add_subdirectory("${MOTO_UPSTREAM_ROOT}/shared/nav_ui" "nav_ui")
if(MSVC)
    # The upstream UI target supplies GCC flags. Translate them here so the
    # imported build file can remain unchanged.
    set_property(TARGET moto_nav_ui PROPERTY COMPILE_OPTIONS /utf-8 /W4 /GR-)
endif()
add_subdirectory("${MOTO_UPSTREAM_ROOT}/shared/nav_presenter" "nav_presenter")

enable_testing()
add_subdirectory("${MOTO_UPSTREAM_ROOT}/tests/coordinates" "coordinate_tests")
add_subdirectory("${MOTO_UPSTREAM_ROOT}/tests/native" "native_tests")

add_library(ducati_boot_animation STATIC
    firmware/main/ducati_boot_animation.cpp
    firmware/assets/boot/ducati_boot_assets.c
)
target_include_directories(ducati_boot_animation PUBLIC
    firmware/main
    firmware/assets/boot
    "${MOTO_UPSTREAM_ROOT}/shared/nav_ui/include"
)
target_link_libraries(ducati_boot_animation PUBLIC lvgl)
add_executable(boot_animation_render_tests tests/boot_animation_render_tests.cpp)
target_link_libraries(boot_animation_render_tests PRIVATE ducati_boot_animation moto_nav_ui)
add_test(NAME boot_animation_render_tests COMMAND boot_animation_render_tests)
