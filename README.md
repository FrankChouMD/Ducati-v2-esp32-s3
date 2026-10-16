# Ducati V2 导航车机

个人非商业自用项目，目标硬件为 **Waveshare ESP32-S3-Touch-AMOLED-2.16**，
配合 iPhone 使用，并保持 Cardo 耳机直接连接手机的音频方案。

本仓库已导入 **MOTO GPS · Waveshare Edition** 的固定版本源码。
原项目作者为 **Maler X**，来源：
[mx3353672833-debug/moto-gps-waveshare](https://github.com/mx3353672833-debug/moto-gps-waveshare)。

## 当前状态

- 已导入上游源码、iOS App、ESP32 固件、Web 调试工具、后端、测试、文档和资源。
- 已保留原始署名、许可证、第三方声明、字体与地图数据的许可文件。
- 已将上游固定的 **LVGL 9.5.0** 子模块展开为普通文件，随本仓库保存。
- 导入基线 commit 为 `9a56ed0`，逐文件记录原始来源和校验值；后续改动见适配记录。
- 已新增 **2.16 板子固件入口**：480×480 CO5300、板级触摸、GPIO16 电源键、16MB 分区和 QMI8658 接入。
- 已接入确认的杜卡迪开机动画：红白盾标、**PANIGALE V2S 逐字渐入**，总时长 2.4 秒。
- 已在导航页和速度页显示当前左右倾角与同步倾斜的 E 版车尾图，接入 QMI8658 原型估算、手机速度补偿与静止零点校准。
- 已按杜卡迪仪表参考统一黑 / 白 / 红配色，使用窄体标签和大号斜体读数，见 [仪表样式](docs/INSTRUMENT_STYLE.md)。
- 已按确认效果图完成导航页四角布局：左上方向、右上限速、左下速度、右下倾角，摩托车后轮接地点居中并与地图投影原点对齐。
- iPhone 页面已改为与车机一致的黑 / 白 / 红主题，包括出发、路线选择、导航中和配套设置页，见 [手机 UI 与效果图](docs/IOS_UI.md)。当前效果图为布局预览，尚未通过 Xcode 构建或真机验收。
- 2.16 固件已通过 ESP-IDF 5.5.5 编译；Windows / 480×480 桌面测试 12 项通过，尚未接板实测。
- Windows 可双击 [run-local.cmd](run-local.cmd)，运行同一份 LVGL 代码的开机动画和导航演示。
- 已新增 [南京骑行辅助规则](docs/NANJING_MOTORCYCLE_NAVIGATION.md)：按南京郊区登记号牌配置，采用摩途官网 124 点禁外摩图层，自动避让、剔除穿区路线并在偏航重算时复查，支持自定义区域、途经点和已收录桥隧规则。
- 手机路线地图显示摩途禁外摩区，规则覆盖仍不完整；限行电子眼数据尚未取得并显示为未加载。来源与数据处理见 [边界核验记录](docs/research/NANJING_BOUNDARY_RESEARCH.md)。
- 算路仍使用高德普通驾车接口；专用摩托车接口、倾角记录与手机回放尚未接入。

杜卡迪开机动画已写入本项目固件；倾角记录和手机回放尚未实现。
编译与桌面测试不代表屏幕、触摸、iPhone 后台连接或 Cardo 组合已通过硬件验证。

## 代码入口

| 内容 | 位置 |
| --- | --- |
| 上游项目说明 | [third_party/moto-gps-waveshare/README.md](third_party/moto-gps-waveshare/README.md) |
| 2.16 板子固件 / 构建与烧录 | [firmware/README.md](firmware/README.md) |
| 2.16 适配内容与验证记录 | [docs/BOARD_2_16_ADAPTATION.md](docs/BOARD_2_16_ADAPTATION.md) |
| 开机动画 / 已确认预览 | [docs/BOOT_ANIMATION.md](docs/BOOT_ANIMATION.md) |
| Windows 本机模拟器 / 测试说明 | [docs/LOCAL_TESTING.md](docs/LOCAL_TESTING.md) |
| 倾角显示 / 安装与校准 | [docs/LEAN_ANGLE.md](docs/LEAN_ANGLE.md) |
| 上游 1.75C 固件 | [platforms/esp32](third_party/moto-gps-waveshare/platforms/esp32/README.md) |
| iPhone App | [platforms/ios](third_party/moto-gps-waveshare/platforms/ios/README.md) |
| 高德路线网关 | [backend](third_party/moto-gps-waveshare/backend/README.md) |
| 共享导航逻辑和界面 | [shared](third_party/moto-gps-waveshare/shared) |
| LVGL 源码 | [third_party/lvgl](third_party/moto-gps-waveshare/third_party/lvgl) |
| 上游已知问题 | [KNOWN_ISSUES.md](third_party/moto-gps-waveshare/docs/KNOWN_ISSUES.md) |
| 导入过程与验证记录 | [docs/MOTO_GPS_IMPORT.md](docs/MOTO_GPS_IMPORT.md) |
| 固定版本及逐文件清单 | [moto-gps-waveshare.upstream.json](third_party/moto-gps-waveshare.upstream.json) |

## 开发环境

本项目的 2.16 固件使用 ESP-IDF **5.5.5**、微雪 **2.16 BSP 2.0.1** 和固定的 LVGL **9.5.0**。
在仓库根目录激活 ESP-IDF 后运行 `idf.py -C firmware build`，详情见固件说明。
根目录 CMake 提供真实 LVGL 的 480×480 桌面检查；上游 1.75C 入口继续保留默认 466×466。
iPhone App 的构建与安装见 iOS 目录说明，需要 macOS / Xcode 和自己的签名配置。

这里保存的是普通文件快照。LVGL 已展开，无需在本仓库执行上游教程中的
`git submodule update --init --recursive`。上游的 `.gitmodules` 留作来源记录，
不会成为当前仓库的子模块配置。ESP-IDF 托管组件、Node 依赖及开发工具链仍需按对应说明安装。

## 自用功能目标

1. 已接入杜卡迪主题启动动画，待接板验证。
2. 在 2.16 英寸屏幕显示转向信息和简化路线，完善摩托车路线接入。
3. 保存原始 IMU 与估计倾角，并在 iPhone 中同步、分析和回放。
4. 车机通过 BLE 传数据，导航语音、音乐和通话继续由 iPhone 与 Cardo 处理。

## 许可与署名

MOTO GPS 原创代码和素材采用 **PolyForm Noncommercial License 1.0.0**，
属于允许非商业使用的源码公开项目。本仓库中的新增整合说明也采用相同许可。
许可证正文见 [LICENSE.md](LICENSE.md)，作者必需署名见 [NOTICE](NOTICE)。
个人研究、实验、爱好与自用属于许可列明的允许用途。

再分发源码、修改版本或固件 / App 时，应一并保留原始许可和必需署名。
LVGL、微雪派生驱动、字体、OSM 数据及其他第三方材料继续适用各自许可，
详见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
PolyForm 的非商业限制不会替代或重新限制这些第三方许可证。

高德、Apple Music、Apple MapKit 等服务的账号、Key 和使用授权由各服务方单独规定。
请使用自己的配置；本项目的源码许可不授权这些外部服务。
