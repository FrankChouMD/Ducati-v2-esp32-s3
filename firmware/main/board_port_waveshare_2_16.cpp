// SPDX-License-Identifier: Apache-2.0
// Adapted from Maler X's MOTO GPS board_port_waveshare_1_75c.cpp and
// Waveshare's esp32_s3_touch_amoled_2_16 BSP (2.0.1).
// Modified 2026-10-14 for the Ducati V2 integration. Original attribution
// and Apache-2.0 license are retained in NOTICE / THIRD_PARTY_NOTICES.md.

#include "board_port.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "bsp/touch.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_co5300.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"

namespace {
constexpr char kTag[] = "ducati_board_2_16";
constexpr gpio_num_t kPowerButtonGpio = GPIO_NUM_16;
constexpr std::uint16_t kDrawBufferHeight = 20;
constexpr int kTransactionQueueDepth = 1;
constexpr std::size_t kLvglPoolBytes = 2U * 1024U * 1024U;
constexpr std::uint16_t kAxp2101Address = 0x34;
constexpr std::uint8_t kCommonConfigRegister = 0x10;
constexpr std::uint8_t kSoftwarePowerOffMask = 0x01;
constexpr std::uint8_t kPowerOffEnableRegister = 0x22;
constexpr std::uint8_t kLongPressShutdownMask = 0x02;
constexpr std::uint8_t kLongPressRestartMask = 0x01;
constexpr std::uint8_t kKeyTimingRegister = 0x27;
constexpr std::uint8_t kPowerOffTimingMask = 0x0C;

static_assert(MOTO_DISPLAY_WIDTH == 480 && MOTO_DISPLAY_HEIGHT == 480,
              "Select the 480x480 shared canvas for this board");
static_assert(MOTO_DISPLAY_WIDTH == BSP_LCD_H_RES &&
                  MOTO_DISPLAY_HEIGHT == BSP_LCD_V_RES,
              "The 2.16 BSP and shared canvas must agree");
static_assert(MOTO_POWER_BUTTON_WAKE_GPIO == 16,
              "GPIO3 is SD data on 2.16; SYS_OUT/wakeup must use GPIO16");
static_assert(kLvglPoolBytes <= LV_MEM_POOL_EXPAND_SIZE,
              "LVGL must accept the additional PSRAM pool");

lv_display_t* display = nullptr;
esp_lcd_panel_handle_t panel = nullptr;
i2c_master_dev_handle_t pmic = nullptr;
void* lvgl_pool = nullptr;
bool display_revealed = false;

// Supplier's 480x480 command sequence. Brightness/DISPON are deferred until
// app_main has flushed its first deliberate black frame. The 1.75C's six-
// pixel column offset and 466-pixel address window do not apply here.
constexpr std::uint8_t kPage20[] = {0x20};
constexpr std::uint8_t kGateVoltage[] = {0x10};
constexpr std::uint8_t kSourceVoltage[] = {0xA0};
constexpr std::uint8_t kPage00[] = {0x00};
constexpr std::uint8_t kGammaControl[] = {0x80};
constexpr std::uint8_t kRgb565[] = {0x55};
constexpr std::uint8_t kTearingEffect[] = {0x00};
constexpr std::uint8_t kWriteControl[] = {0x20};
constexpr std::uint8_t kBrightnessOff[] = {0x00};
constexpr std::uint8_t kRegister63[] = {0xFF};
constexpr std::uint8_t kAddressRange[] = {0x00, 0x00, 0x01, 0xDF};
constexpr std::uint8_t kMemoryAccess[] = {0xA0};
const co5300_lcd_init_cmd_t kHiddenPanelInitCommands[] = {
    {0x11, nullptr, 0, 600},
    {0xFE, kPage20, sizeof(kPage20), 0},
    {0x19, kGateVoltage, sizeof(kGateVoltage), 0},
    {0x1C, kSourceVoltage, sizeof(kSourceVoltage), 0},
    {0xFE, kPage00, sizeof(kPage00), 0},
    {0xC4, kGammaControl, sizeof(kGammaControl), 0},
    {0x3A, kRgb565, sizeof(kRgb565), 0},
    {0x35, kTearingEffect, sizeof(kTearingEffect), 0},
    {0x53, kWriteControl, sizeof(kWriteControl), 0},
    {0x51, kBrightnessOff, sizeof(kBrightnessOff), 0},
    {0x63, kRegister63, sizeof(kRegister63), 0},
    {0x2A, kAddressRange, sizeof(kAddressRange), 0},
    {0x2B, kAddressRange, sizeof(kAddressRange), 0},
    {0x36, kMemoryAccess, sizeof(kMemoryAccess), 0},
    {0x28, nullptr, 0, 0},
};

esp_err_t initialize_hidden_panel(esp_lcd_panel_io_handle_t* io_out) {
  spi_bus_config_t bus{};
  bus.data0_io_num = BSP_LCD_DATA0;
  bus.data1_io_num = BSP_LCD_DATA1;
  bus.sclk_io_num = BSP_LCD_PCLK;
  bus.data2_io_num = BSP_LCD_DATA2;
  bus.data3_io_num = BSP_LCD_DATA3;
  bus.data4_io_num = -1;
  bus.data5_io_num = -1;
  bus.data6_io_num = -1;
  bus.data7_io_num = -1;
  bus.max_transfer_sz = MOTO_DISPLAY_WIDTH * kDrawBufferHeight *
                        MOTO_DISPLAY_BITS_PER_PIXEL / 8;
  ESP_RETURN_ON_ERROR(spi_bus_initialize(BSP_LCD_SPI_NUM, &bus, SPI_DMA_CH_AUTO),
                      kTag, "QSPI bus initialization failed");

  esp_lcd_panel_io_spi_config_t io_config{};
  io_config.cs_gpio_num = BSP_LCD_CS;
  io_config.dc_gpio_num = -1;
  io_config.spi_mode = 0;
  io_config.pclk_hz = 40 * 1'000 * 1'000;
  io_config.trans_queue_depth = kTransactionQueueDepth;
  io_config.lcd_cmd_bits = 32;
  io_config.lcd_param_bits = 8;
  io_config.flags.quad_mode = true;
  // Use internal RAM for color DMA. Direct PSRAM DMA underflows on the
  // board when Bluetooth starts sharing memory bandwidth.
  io_config.flags.psram_dma_direct = false;
  esp_lcd_panel_io_handle_t io = nullptr;
  ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi(
                          static_cast<esp_lcd_spi_bus_handle_t>(BSP_LCD_SPI_NUM),
                          &io_config, &io),
                      kTag, "QSPI panel IO initialization failed");

  co5300_vendor_config_t vendor{};
  vendor.init_cmds = kHiddenPanelInitCommands;
  vendor.init_cmds_size = sizeof(kHiddenPanelInitCommands) /
                          sizeof(kHiddenPanelInitCommands[0]);
  vendor.flags.use_qspi_interface = 1;
  esp_lcd_panel_dev_config_t panel_config{};
  panel_config.reset_gpio_num = BSP_LCD_RST;
  panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
  panel_config.bits_per_pixel = BSP_LCD_BITS_PER_PIXEL;
  panel_config.vendor_config = &vendor;
  ESP_RETURN_ON_ERROR(esp_lcd_new_panel_co5300(io, &panel_config, &panel), kTag,
                      "CO5300 panel creation failed");
  ESP_RETURN_ON_ERROR(esp_lcd_panel_set_gap(panel, 0, 0), kTag,
                      "CO5300 zero-offset setup failed");
  ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel), kTag, "CO5300 reset failed");
  ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel), kTag, "CO5300 init failed");
  ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(panel, false), kTag,
                      "CO5300 blanking failed");
  *io_out = io;
  return ESP_OK;
}

void round_draw_area(lv_area_t* area, void*) {
  area->x1 = std::max<std::int32_t>(0, (area->x1 >> 1) << 1);
  area->y1 = std::max<std::int32_t>(0, (area->y1 >> 1) << 1);
  area->x2 = std::min<std::int32_t>(MOTO_DISPLAY_WIDTH - 1,
                                  ((area->x2 >> 1) << 1) + 1);
  area->y2 = std::min<std::int32_t>(MOTO_DISPLAY_HEIGHT - 1,
                                  ((area->y2 >> 1) << 1) + 1);
}

esp_err_t read_pmic(std::uint8_t reg, std::uint8_t* value) {
  return i2c_master_transmit_receive(pmic, &reg, 1, value, 1, 100);
}

esp_err_t write_pmic(std::uint8_t reg, std::uint8_t value) {
  const std::uint8_t command[] = {reg, value};
  return i2c_master_transmit(pmic, command, sizeof(command), 100);
}

esp_err_t initialize_power_control() {
  // Schematic Key1 grounds PWRON. BSS138 inverts that into active-high
  // SYS_OUT on GPIO16. GPIO3 must remain available to the microSD interface.
  gpio_config_t button{};
  button.pin_bit_mask = 1ULL << kPowerButtonGpio;
  button.mode = GPIO_MODE_INPUT;
  button.pull_up_en = GPIO_PULLUP_DISABLE;
  button.pull_down_en = GPIO_PULLDOWN_ENABLE;
  button.intr_type = GPIO_INTR_DISABLE;
  ESP_RETURN_ON_ERROR(gpio_config(&button), kTag, "SYS_OUT setup failed");
  ESP_RETURN_ON_ERROR(bsp_i2c_init(), kTag, "Shared I2C initialization failed");

  i2c_device_config_t device{};
  device.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  device.device_address = kAxp2101Address;
  device.scl_speed_hz = 400'000;
  ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bsp_i2c_get_handle(), &device, &pmic),
                      kTag, "AXP2101 registration failed");
  // Preserve charging and rail policy; change only the established upstream
  // 4-second hardware shutdown timing and long-press restart/shutdown bits.
  std::uint8_t value = 0;
  ESP_RETURN_ON_ERROR(read_pmic(kKeyTimingRegister, &value), kTag,
                      "AXP2101 key timing read failed");
  value = static_cast<std::uint8_t>(value & ~kPowerOffTimingMask);
  ESP_RETURN_ON_ERROR(write_pmic(kKeyTimingRegister, value), kTag,
                      "AXP2101 key timing write failed");
  ESP_RETURN_ON_ERROR(read_pmic(kKeyTimingRegister, &value), kTag,
                      "AXP2101 key timing verification failed");
  if ((value & kPowerOffTimingMask) != 0) return ESP_ERR_INVALID_RESPONSE;
  ESP_RETURN_ON_ERROR(read_pmic(kPowerOffEnableRegister, &value), kTag,
                      "AXP2101 shutdown policy read failed");
  value = static_cast<std::uint8_t>((value | kLongPressShutdownMask) &
                                   ~kLongPressRestartMask);
  ESP_RETURN_ON_ERROR(write_pmic(kPowerOffEnableRegister, value), kTag,
                      "AXP2101 shutdown policy write failed");
  ESP_RETURN_ON_ERROR(read_pmic(kPowerOffEnableRegister, &value), kTag,
                      "AXP2101 shutdown policy verification failed");
  if ((value & (kLongPressShutdownMask | kLongPressRestartMask)) !=
      kLongPressShutdownMask) return ESP_ERR_INVALID_RESPONSE;
  return ESP_OK;
}
}  // namespace

extern "C" esp_err_t board_port_init(void) {
  if (display != nullptr) return ESP_OK;
  const esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
  ESP_RETURN_ON_ERROR(esp_lv_adapter_init(&adapter_config), kTag,
                      "LVGL adapter initialization failed");
  static_assert(LV_USE_STDLIB_MALLOC == LV_STDLIB_BUILTIN,
                "Revisit the PSRAM pool if changing the LVGL allocator");
  lvgl_pool = heap_caps_malloc(kLvglPoolBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (lvgl_pool == nullptr || lv_mem_add_pool(lvgl_pool, kLvglPoolBytes) == nullptr) {
    heap_caps_free(lvgl_pool);
    lvgl_pool = nullptr;
    return ESP_ERR_NO_MEM;
  }
  esp_lcd_panel_io_handle_t io = nullptr;
  ESP_RETURN_ON_ERROR(initialize_hidden_panel(&io), kTag,
                      "2.16 display initialization failed");
  esp_lv_adapter_display_config_t config =
      ESP_LV_ADAPTER_DISPLAY_SPI_WITH_PSRAM_DEFAULT_CONFIG(
          panel, io, MOTO_DISPLAY_WIDTH, MOTO_DISPLAY_HEIGHT,
          ESP_LV_ADAPTER_ROTATE_0);
  config.profile.buffer_height = kDrawBufferHeight;
  config.profile.require_double_buffer = false;
  config.profile.use_psram = false;
  display = esp_lv_adapter_register_display(&config);
  if (display == nullptr) return ESP_FAIL;
  ESP_RETURN_ON_ERROR(esp_lv_adapter_set_area_rounder_cb(display, round_draw_area, nullptr),
                      kTag, "CO5300 area rounding setup failed");

  // The 2.16 official BSP uses its CST9217-compatible driver for CST9220,
  // GPIO40 touch reset and swap_xy=1 / mirror_y=1. Use that board-specific
  // setup rather than the 1.75C's GPIO2 reset and mirror_x=1 configuration.
  bsp_display_cfg_t touch_config{};
  touch_config.lv_adapter_cfg = adapter_config;
  touch_config.rotation = ESP_LV_ADAPTER_ROTATE_0;
  touch_config.tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE;
  touch_config.touch_flags.swap_xy = 1;
  touch_config.touch_flags.mirror_x = 0;
  touch_config.touch_flags.mirror_y = 1;
  esp_lcd_touch_handle_t touch = nullptr;
  ESP_RETURN_ON_ERROR(bsp_touch_new(&touch_config, &touch), kTag,
                      "2.16 CST9220-compatible touch initialization failed");
  const esp_lv_adapter_touch_config_t input_config =
      ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(display, touch);
  if (esp_lv_adapter_register_touch(&input_config) == nullptr) return ESP_FAIL;
  ESP_RETURN_ON_ERROR(initialize_power_control(), kTag,
                      "2.16 power control initialization failed");

  lv_obj_t* screen = lv_display_get_screen_active(display);
  lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  ESP_RETURN_ON_ERROR(esp_lv_adapter_start(), kTag, "LVGL worker startup failed");
  // GPIO13 is RTC_INT on 2.16. No TE GPIO handler or frame gate is installed.
  ESP_LOGI(kTag, "480x480 CO5300 / CST9220 ready; PWR GPIO16; TE gating disabled");
  return ESP_OK;
}

extern "C" lv_display_t* board_port_get_display(void) { return display; }

extern "C" esp_err_t board_port_reveal_display(void) {
  if (panel == nullptr || display == nullptr) return ESP_ERR_INVALID_STATE;
  if (display_revealed) return ESP_OK;
  ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(panel, true), kTag,
                      "AMOLED reveal failed");
  ESP_RETURN_ON_ERROR(esp_lcd_panel_co5300_set_brightness(panel, 100), kTag,
                      "AMOLED brightness restore failed");
  display_revealed = true;
  return ESP_OK;
}

extern "C" bool board_port_lock(uint32_t timeout_ms) {
  if (display == nullptr) return false;
  // Adapter returns esp_err_t, not bool. Call it directly because the BSP's
  // bool wrapper in 2.0.1 forwards ESP_OK as false.
  const std::int32_t timeout = timeout_ms == UINT32_MAX
      ? -1 : static_cast<std::int32_t>(std::min<std::uint32_t>(
                 timeout_ms, std::numeric_limits<std::int32_t>::max()));
  return esp_lv_adapter_lock(timeout) == ESP_OK;
}

extern "C" void board_port_unlock(void) {
  if (display != nullptr) esp_lv_adapter_unlock();
}

extern "C" bool board_port_power_button_pressed(void) {
  return gpio_get_level(kPowerButtonGpio) != 0;
}

extern "C" esp_err_t board_port_power_off(void) {
  if (pmic == nullptr) return ESP_ERR_INVALID_STATE;
  if (panel != nullptr) {
    const esp_err_t result = esp_lcd_panel_disp_on_off(panel, false);
    if (result != ESP_OK) ESP_LOGW(kTag, "AMOLED blanking failed: %s", esp_err_to_name(result));
  }
  std::uint8_t value = 0;
  ESP_RETURN_ON_ERROR(read_pmic(kCommonConfigRegister, &value), kTag,
                      "AXP2101 power-off register read failed");
  return write_pmic(kCommonConfigRegister,
                    static_cast<std::uint8_t>(value | kSoftwarePowerOffMask));
}
