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
- 导入阶段保持上游文件内容不变，逐文件记录来源和校验值。
- 当前上游固件适配 **ESP32-S3-Touch-AMOLED-1.75C**；**2.16 板子适配尚未完成**。
- 当前上游使用高德普通驾车路线服务；摩托车路线、倾角记录与手机回放需要继续开发。

导入校验只确认文件完整性和许可材料保留情况，不代表固件、iPhone 后台连接或实车功能已通过验证。

## 代码入口

| 内容 | 位置 |
| --- | --- |
| 上游项目说明 | [third_party/moto-gps-waveshare/README.md](third_party/moto-gps-waveshare/README.md) |
| ESP32 固件 | [platforms/esp32](third_party/moto-gps-waveshare/platforms/esp32/README.md) |
| iPhone App | [platforms/ios](third_party/moto-gps-waveshare/platforms/ios/README.md) |
| 高德路线网关 | [backend](third_party/moto-gps-waveshare/backend/README.md) |
| 共享导航逻辑和界面 | [shared](third_party/moto-gps-waveshare/shared) |
| LVGL 源码 | [third_party/lvgl](third_party/moto-gps-waveshare/third_party/lvgl) |
| 上游已知问题 | [KNOWN_ISSUES.md](third_party/moto-gps-waveshare/docs/KNOWN_ISSUES.md) |
| 导入过程与验证记录 | [docs/MOTO_GPS_IMPORT.md](docs/MOTO_GPS_IMPORT.md) |
| 固定版本及逐文件清单 | [moto-gps-waveshare.upstream.json](third_party/moto-gps-waveshare.upstream.json) |

## 开发环境

上游 ESP32 固件使用 ESP-IDF **5.5.5** 和微雪 **1.75C BSP 3.0.0**。
请先阅读固件目录的说明；后续适配 2.16 时需要修改板级驱动、触摸、显示尺寸和 Flash 分区。
iPhone App 的构建与安装见 iOS 目录说明，需要 macOS / Xcode 和自己的签名配置。

这里保存的是普通文件快照。LVGL 已展开，无需在本仓库执行上游教程中的
`git submodule update --init --recursive`。上游的 `.gitmodules` 留作来源记录，
不会成为当前仓库的子模块配置。ESP-IDF 托管组件、Node 依赖及开发工具链仍需按对应说明安装。

## 自用功能目标

1. 将启动画面替换为杜卡迪主题动画。
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
