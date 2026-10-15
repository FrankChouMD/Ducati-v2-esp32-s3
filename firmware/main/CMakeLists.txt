set(MOTO_UPSTREAM_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../third_party/moto-gps-waveshare")
set(MOTO_ESP32_MAIN "${MOTO_UPSTREAM_ROOT}/platforms/esp32/main")

idf_component_register(
    SRCS
        "board_port_waveshare_2_16.cpp"
        "ducati_boot_animation.cpp"
        "../assets/boot/ducati_boot_assets.c"
        "${MOTO_ESP32_MAIN}/app_main.cpp"
        "${MOTO_ESP32_MAIN}/ble_nav_transport_nimble.cpp"
        "${MOTO_ESP32_MAIN}/motion_heading_sensor.cpp"
        "${MOTO_ESP32_MAIN}/phone_nav_bridge.cpp"
        "${MOTO_UPSTREAM_ROOT}/shared/ble_protocol/src/ble_protocol.cpp"
        "${MOTO_UPSTREAM_ROOT}/shared/nav_presenter/src/moto_nav_presenter.cpp"
        "${MOTO_UPSTREAM_ROOT}/shared/nav_ui/assets/moto_font_nav_16.c"
        "${MOTO_UPSTREAM_ROOT}/shared/nav_ui/assets/ducati_rear_e.c"
        "${MOTO_UPSTREAM_ROOT}/shared/nav_ui/assets/fonts/ducati_label_18.c"
        "${MOTO_UPSTREAM_ROOT}/shared/nav_ui/assets/fonts/ducati_label_22.c"
        "${MOTO_UPSTREAM_ROOT}/shared/nav_ui/assets/fonts/ducati_readout_32.c"
        "${MOTO_UPSTREAM_ROOT}/shared/nav_ui/assets/fonts/ducati_readout_56.c"
        "${MOTO_UPSTREAM_ROOT}/shared/nav_ui/assets/fonts/ducati_readout_88.c"
        "${MOTO_UPSTREAM_ROOT}/shared/nav_ui/src/moto_nav_ui.cpp"
    INCLUDE_DIRS
        "."
        "../assets/boot"
        "${MOTO_ESP32_MAIN}"
        "${MOTO_UPSTREAM_ROOT}/shared"
        "${MOTO_UPSTREAM_ROOT}/shared/ble_protocol/include"
        "${MOTO_UPSTREAM_ROOT}/shared/nav_presenter/include"
        "${MOTO_UPSTREAM_ROOT}/shared/nav_ui/include"
        "${MOTO_UPSTREAM_ROOT}/shared/nav_ui/assets"
    REQUIRES
        esp_timer esp_driver_gpio esp_driver_i2c esp_driver_spi
        esp_hw_support freertos log lvgl esp_lcd_co5300
        esp_lvgl_adapter nvs_flash bt esp32_s3_touch_amoled_2_16
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
target_compile_definitions(${COMPONENT_LIB} PRIVATE DUCATI_BOOT_ANIMATION=1)

set(DUCATI_IMU_SCREEN_RIGHT_AXIS "1" CACHE STRING "IMU axis toward screen right: 1/-1=X, 2/-2=Y")
if(NOT DUCATI_IMU_SCREEN_RIGHT_AXIS MATCHES "^-?[12]$")
    message(FATAL_ERROR "DUCATI_IMU_SCREEN_RIGHT_AXIS must be 1, -1, 2 or -2")
endif()
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    MOTO_IMU_SCREEN_RIGHT_AXIS=${DUCATI_IMU_SCREEN_RIGHT_AXIS})
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
)
