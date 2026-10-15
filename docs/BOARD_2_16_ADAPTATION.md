# ESP32-S3-Touch-AMOLED-2.16 第一阶段适配

日期：2026-10-14。导入基线：`9a56ed0`。
更新：2026-10-15 接入已确认的杜卡迪 / PANIGALE V2S 开机动画。
同日按方屏修正导航底部进度条，移除原圆屏的周边弧线。
同日接入导航页 / 速度页倾角显示、QMI8658 原型估算和静止零点校准。
倾角数值旁增加按同一读数转动的 E 版摩托车车尾图和直立参考线。
同日按参考图统一黑 / 白 / 红仪表配色，改用 Barlow Condensed 窄体与斜体读数。
按确认效果图将导航改为四角仪表与底部居中摩托车，并同步调整地图投影原点。
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
| IMU | QMI8658，地址 0x6B | 同一采样任务用于航向辅助和倾角估算，未实现倾角日志 |
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

导入代码中的主要修改如下，均保留原署名和适用许可：

| 文件（相对导入目录） | 改动 |
| --- | --- |
| `shared/nav_ui/include/moto_nav_ui.h` | 可覆盖画布尺寸与 `MOTO_UI_SQUARE_SCREEN`；共享地图原点 / 高度；倾角状态及校准回调 |
| `shared/nav_ui/src/moto_nav_ui.cpp` | 方屏四角导航 HUD、居中摩托车与长按校准；水平进度条、分页位置、独立速度页；修复字体链接和重复创建时的旧定时器残留 |
| `shared/nav_ui/assets/ducati_rear_e.*` | 新增选定的 E 版车尾图，86×128 ARGB8888；围绕后轮接地点随倾角转动 |
| `shared/nav_ui/assets/fonts/` | 新增 OFL 字体子集，标签 18 / 22px、斜体读数 32 / 56 / 88px |
| `shared/nav_ui/CMakeLists.txt`、`platforms/esp32/main/CMakeLists.txt` | 将共享 E 版图标和仪表字体加入桌面 / Web 和原 ESP32 构建入口 |
| `platforms/esp32/main/board_port.h` | 新增可覆盖的电源唤醒脚宏，默认保留 GPIO3 |
| `platforms/esp32/main/app_main.cpp` | GPIO16 唤醒、2.16 杜卡迪动画；传递完整六轴采样 |
| `platforms/esp32/main/motion_heading_sensor.h/.cpp` | 复用采样任务向航向 / 倾角提供六轴数据，检测配置错误和断流 |
| `platforms/esp32/main/phone_nav_bridge.h/.cpp` | 倾角估算、速度参考、独立刷新、校准与演示隔离 |
| `shared/nav_core/nav_core.hpp`、`shared/nav_presenter/src/moto_nav_presenter.cpp` | 本地倾角状态 / 有效性映射，地图原点与后轮接地点一致，不改 BLE 线格式 |
| `tests/native/` | 扩展桩、映射、真实 UI 渲染与桥接检查以覆盖倾角和校准 |

新增 `motion_imu_sample.hpp` 与 `motion_lean_fusion.hpp` 位于共享的 ESP32 代码目录，
属于本项目整合代码，不是原始导入快照。算法、安装配置与限制见 [LEAN_ANGLE.md](LEAN_ANGLE.md)。

根目录 CMake 用于 480×480 的真实 LVGL 桌面检查，修正 MSVC 的编译参数与 UTF-8 编码。
2.16 固件和 480×480 桌面目标均开启 `MOTO_UI_SQUARE_SCREEN=1`。
导航路线进度保留百分比与路况颜色，限速交通标志继续使用红色圆环；
466×466 目标保留上游圆屏进度条。
原始 1.75C 板级文件和固件配置保持原实现，其 CMake 增加共享图标 / 字体源文件与头文件目录。
iPhone App、BLE 协议和导航数据格式保持原实现。
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
用户选定的 E 版车尾图来源与生成记录见 [倾角素材说明](../firmware/assets/lean/README.md)。
仪表的 Barlow Condensed 字体和生成子集适用独立的 SIL OFL 1.1，见 [字体说明](../firmware/assets/fonts/README.md)。
许可正文与署名见 [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) 和 [NOTICE](../NOTICE)。

## 验证记录

- ESP-IDF 5.5.5 / esp32s3：完整固件、bootloader、分区表编译与链接通过。
- 完成四角导航布局、地图原点对齐和定时器清理后的应用二进制为 `0x175f80`（1,531,776）字节，小于 8MiB factory 应用区。
- Windows / MSVC / 480×480：12 项 CTest 全部通过，包含本机模拟器、启动动画和真实 LVGL 渲染、导航状态、
  坐标转换、BLE 编解码、连接隔离、航向融合和手机桥接。
- 真实 LVGL 页面像素核对：旧底部弧线已移除；进度条位于 `(24,461)`、大小 432×5，25% 进度填充 108 像素。
- 新布局核对四角读数、`(240,424)` 地图 / 后轮共同原点、160 km/h、限速缺失、12.3 km 和重复创建 UI 的定时器清理。
- 倾角检查覆盖左右持续转弯、加速度补偿、校准、丢样、无效值、速度过期与双页显示；尚无实车精度数据。
- 真实 LVGL 图形像素核对左右 30°、直立、右倾 85° 与失效状态；E 版车尾图和数值同步，失效后隐藏图标。
- 黑 / 白 / 红仪表配色、窄体斜体数字及三个页面的导出预览见 [INSTRUMENT_STYLE.md](INSTRUMENT_STYLE.md)。
- 真实 LVGL 渲染程序按 320 行绘制缓冲和额外 2MiB 内存池运行通过；
  此项仍使用主机的 8MiB基础堆，不等同于板载 SRAM / PSRAM 的容量和时序验收。
- 构建存在上游 MSVC 数值收窄警告及 SDK / 组件 C++ 扩展与默认初始化警告；没有编译错误。

本次没有刷写开发板，也未构建 iOS App。主机测试不验证板载 PSRAM、DMA 时序或 I2C 外设响应。

## 接板验收与后续功能

1. 串口确认 480×480、16MB Flash、8MB PSRAM，检查初始黑帧与界面稳定性。
2. 检查四角触摸和滑动方向；核对屏幕颜色、边缘像素、刷新撕裂与触摸抖动。
3. 分别在 USB、电池和 USB + 电池供电下验证开机、短按、长按、关机与唤醒。
4. 验证 QMI8658 初始化、静止零点、航向辅助与左右倾角轴向；验证实车估算后开发带时间戳记录。
5. iPhone 连接、导航、锁屏与重连；同时连接 Cardo，验证导航语音、音乐和通话。
6. 验证杜卡迪动画的完整播放及导航切换；实现摩托车路线方案、骑行日志同步和手机回放。

2.16 目标已接入杜卡迪盾标与 PANIGALE V2S 逐字渐入动画，详情见 [BOOT_ANIMATION.md](BOOT_ANIMATION.md)。
导航仍使用普通驾车路线服务；microSD 日志、倾角回放与 CarPlay 尚未实现。
上游 iPhone 后台恢复的已知问题仍需处理，见
[KNOWN_ISSUES.md](../third_party/moto-gps-waveshare/docs/KNOWN_ISSUES.md)。
