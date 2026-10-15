# ESP32-S3-Touch-AMOLED-2.16 第一阶段适配

日期：2026-10-14。导入基线：`9a56ed0`。
更新：2026-10-15 接入已确认的杜卡迪 / PANIGALE V2S 开机动画。
本阶段新增独立 `firmware/` 入口，并将共享 UI 的画布尺寸和电源唤醒脚参数化。
目标是使现有 MOTO GPS 导航程序能在 2.16 板上构建，再进行接板验收。

## 硬件依据

核对了 [Waveshare 产品文档](https://docs.waveshare.net/ESP32-S3-Touch-AMOLED-2.16/)、
[官方示例与原理图](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.16/tree/225a62bff11b5d0a0b607873860d39485a9a9685) 和
[官方 BSP 源码](https://github.com/waveshareteam/Waveshare-ESP32-components/tree/23a6012d5e87d27399cf90fb851cab3689e29159/bsp/esp32_s3_touch_amoled_2_16)。
原理图的电源键电路与 MCU 网络连接已渲染查看；参考资料下载到忽略目录 `build/reference/`。

| 设施 | 2.16 配置 | 适配方式 |
| --- | --- | --- |
| AMOLED | CO5300，480×480，RGB565 | 使用 480 地址范围，移除 1.75C 的 6 像素列偏移 |
| QSPI 数据 | D0=4、D1=5、D2=6、D3=7 | 使用 2.16 BSP 引脚宏 |
| QSPI 控制 | CLK=38、CS=12、RESET=39 | 40MHz、单事务队列、PSRAM 双缓冲 |
| 触摸 | CST9220，RESET=40、INT=11 | 使用官方 BSP 的 CST9217 兼容驱动；swap_xy=1、mirror_y=1 |
| 共享 I2C | SDA=15、SCL=14 | BSP 建立总线，触摸 / PMIC / IMU 复用 |
| IMU | QMI8658，地址 0x6B | 复用上游航向辅助，未实现倾角日志 |
| 电源管理 | AXP2101，地址 0x34 | 保留上游软件关机和 4 秒硬件长按关机策略 |
| PWR / SYS_OUT | GPIO16，按下为高 | 更改按键输入和深睡眠唤醒脚 |
| GPIO13 | RTC_INT | 不安装上游的 TE 中断或帧同步门控 |
| Flash / PSRAM | 16MB QIO / 8MB octal PSRAM | 更新默认配置与 16MB 分区表 |

PWR Key1 将 PWRON 拉低，BSS138 电路将信号反相后送入 GPIO16 的 SYS_OUT，
因此沿用高电平按下 / 唤醒逻辑。1.75C 的 GPIO3 在本板供 microSD 数据使用。
屏幕的 TE 命令保持官方初始化值，但没有选择外部 TE GPIO；显示撕裂情况需实测。

## 修改内容

新增 `firmware/main/board_port_waveshare_2_16.cpp` 作为 2.16 专用板级实现，
并提供 ESP-IDF 入口、组件清单 / 锁文件、默认配置、分区表和构建说明。
板级实现保留初始黑屏、首帧后点亮、2MiB LVGL PSRAM 池和 320 行双绘制缓冲。
直接判断 `esp_lv_adapter_lock(...) == ESP_OK`；BSP 2.0.1 的 bool 锁包装直接返回
`esp_err_t`，会将成功值 0 转成 false。

对导入文件的修改只有以下四处，均保留原署名和适用许可：

| 文件（相对导入目录） | 改动 |
| --- | --- |
| `shared/nav_ui/include/moto_nav_ui.h` | 允许编译入口覆盖画布尺寸，默认仍为 466×466 |
| `shared/nav_ui/src/moto_nav_ui.cpp` | 为 C 生成的字体添加明确 C 链接声明，修复 MSVC 链接失败 |
| `platforms/esp32/main/board_port.h` | 新增可覆盖的电源唤醒脚宏，默认保留 GPIO3 |
| `platforms/esp32/main/app_main.cpp` | 使用板级宏选择 GPIO16 唤醒；2.16 目标选择杜卡迪动画并等待完整 2.4 秒 |

根目录 CMake 用于 480×480 的真实 LVGL 桌面检查，修正 MSVC 的编译参数与 UTF-8 编码。
原始 1.75C 板级文件、固件配置、iPhone App、BLE 协议和导航数据格式保持原实现。
原始导入清单继续表示基线原文件；不会把本地修改后的校验值冒充上游原始值。

## 依赖与许可

| 依赖 | 本阶段使用版本 |
| --- | --- |
| ESP-IDF | 5.5.5，提交 `b774170ff46c393eeb5e495ea37936038d3f4f4f` |
| 2.16 BSP | 2.0.1 |
| CO5300 | 2.1.0 |
| esp_lvgl_adapter | 0.6.4 |
| QMI8658 | 2.0.0 |
| LVGL | 9.5.0，导入基线固定版本 |

间接依赖的版本与组件 hash 见 [dependencies.lock](../firmware/dependencies.lock)。
新增板级实现派生自 Maler X 的 Apache-2.0 板级代码和 Waveshare BSP，
保留 Apache-2.0 标记、来源和修改日期；其他新增整合代码和文档沿用根目录非商业许可。
杜卡迪标识保持原权利状态，来源和生成记录见 [启动素材说明](../firmware/assets/boot/README.md)。
许可正文与署名见 [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) 和 [NOTICE](../NOTICE)。

## 验证记录

- ESP-IDF 5.5.5 / esp32s3：完整固件、bootloader、分区表编译与链接通过。
- 加入动画后的应用二进制为 `0x169e90`（1,482,384）字节，小于 8MiB factory 应用区。
- Windows / MSVC / 480×480：10 项 CTest 全部通过，包含启动动画和真实 LVGL 渲染、导航状态、
  坐标转换、BLE 编解码、连接隔离、航向融合和手机桥接。
- 真实 LVGL 渲染程序按 320 行绘制缓冲和额外 2MiB 内存池运行通过；
  此项仍使用主机的 8MiB基础堆，不等同于板载 SRAM / PSRAM 的容量和时序验收。
- 构建存在上游 MSVC 数值收窄警告及 SDK / 组件 C++ 扩展与默认初始化警告；没有编译错误。

本次没有刷写开发板，也未构建 iOS App。主机测试不验证板载 PSRAM、DMA 时序或 I2C 外设响应。

## 接板验收与后续功能

1. 串口确认 480×480、16MB Flash、8MB PSRAM，检查初始黑帧与界面稳定性。
2. 检查四角触摸和滑动方向；核对屏幕颜色、边缘像素、刷新撕裂与触摸抖动。
3. 分别在 USB、电池和 USB + 电池供电下验证开机、短按、长按、关机与唤醒。
4. 验证 QMI8658 初始化、静止偏置与航向辅助，随后开发倾角估计和带时间戳记录。
5. iPhone 连接、导航、锁屏与重连；同时连接 Cardo，验证导航语音、音乐和通话。
6. 验证杜卡迪动画的完整播放及导航切换；实现摩托车路线方案、骑行日志同步和手机回放。

2.16 目标已接入杜卡迪盾标与 PANIGALE V2S 逐字渐入动画，详情见 [BOOT_ANIMATION.md](BOOT_ANIMATION.md)。
导航仍使用普通驾车路线服务；microSD 日志、倾角回放与 CarPlay 尚未实现。
上游 iPhone 后台恢复的已知问题仍需处理，见
[KNOWN_ISSUES.md](../third_party/moto-gps-waveshare/docs/KNOWN_ISSUES.md)。
