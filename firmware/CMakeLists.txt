cmake_minimum_required(VERSION 3.20)

get_filename_component(DUCATI_REPOSITORY_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(MOTO_UPSTREAM_ROOT "${DUCATI_REPOSITORY_ROOT}/third_party/moto-gps-waveshare")
set(MOTO_LVGL_PATH "${MOTO_UPSTREAM_ROOT}/third_party/lvgl")
set(MOTO_LV_CONF_PATH "${MOTO_UPSTREAM_ROOT}/config/lv_conf.h")
set(ENV{LVGL_VERSION} "9.5.0")
set(ENV{DUCATI_LVGL_PATH} "${MOTO_LVGL_PATH}")

if(NOT EXISTS "${MOTO_UPSTREAM_ROOT}/third_party/lvgl/CMakeLists.txt")
    message(FATAL_ERROR "Imported LVGL 9.5.0 source is missing")
endif()
if(NOT DEFINED ENV{IDF_PATH} OR NOT EXISTS "$ENV{IDF_PATH}/tools/cmake/project.cmake")
    message(FATAL_ERROR "Activate ESP-IDF 5.5.5 before configuring this firmware")
endif()

# All shared UI/presenter/phone bridge translation units use one geometry.
add_compile_definitions(
    LV_CONF_PATH="${MOTO_LV_CONF_PATH}"
    MOTO_UI_CANVAS_WIDTH=480
    MOTO_UI_CANVAS_HEIGHT=480
    MOTO_POWER_BUTTON_WAKE_GPIO=16
)
set(IDF_TARGET "esp32s3" CACHE STRING "ESP-IDF chip target")
set(COMPONENTS main)
set(PROJECT_VER "0.1.0")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ducati_v2_amoled_2_16)

# Component Manager 2.5 serializes local sources as machine-specific absolute
# paths. Keep its resolved versions/hashes, but make our vendored LVGL path
# portable. This variable is set above before the next dependency resolution.
set(DUCATI_LOCK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/dependencies.lock")
if(EXISTS "${DUCATI_LOCK_PATH}")
    file(READ "${DUCATI_LOCK_PATH}" DUCATI_LOCK_CONTENT)
    file(TO_NATIVE_PATH "${MOTO_LVGL_PATH}" DUCATI_LVGL_NATIVE_PATH)
    string(REPLACE "path: ${MOTO_LVGL_PATH}" "path: \${DUCATI_LVGL_PATH}"
        DUCATI_PORTABLE_LOCK "${DUCATI_LOCK_CONTENT}")
    string(REPLACE "path: ${DUCATI_LVGL_NATIVE_PATH}" "path: \${DUCATI_LVGL_PATH}"
        DUCATI_PORTABLE_LOCK "${DUCATI_PORTABLE_LOCK}")
    if(NOT DUCATI_PORTABLE_LOCK STREQUAL DUCATI_LOCK_CONTENT)
        file(WRITE "${DUCATI_LOCK_PATH}" "${DUCATI_PORTABLE_LOCK}")
    endif()
endif()
