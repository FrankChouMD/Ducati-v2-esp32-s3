# 第三方许可索引

本仓库保留 MOTO GPS 的原始第三方声明：
[THIRD_PARTY_NOTICES.md](third_party/moto-gps-waveshare/THIRD_PARTY_NOTICES.md)
及其 [英文版本](third_party/moto-gps-waveshare/THIRD_PARTY_NOTICES.en.md)。
以下为入口索引，不替代原始许可证正文和声明。

| 材料 | 原始许可 / 声明 | 本地入口 |
| --- | --- | --- |
| MOTO GPS 原创代码与素材，Maler X | PolyForm Noncommercial 1.0.0 | [许可证](third_party/moto-gps-waveshare/LICENSE.md)、[必需署名](third_party/moto-gps-waveshare/NOTICE) |
| LVGL 固定版本 9.5.0 | MIT | [LICENCE.txt](third_party/moto-gps-waveshare/third_party/lvgl/LICENCE.txt)、[上游保留副本](third_party/moto-gps-waveshare/LICENSES/LVGL-MIT.txt) |
| 微雪 BSP / CO5300 派生板级代码 | Apache-2.0 | [许可正文](third_party/moto-gps-waveshare/LICENSES/Waveshare-Apache-2.0.txt)、[派生文件](third_party/moto-gps-waveshare/platforms/esp32/main/board_port_waveshare_1_75c.cpp) |
| 2.16 板级适配，派生自 Maler X 板级代码与 Waveshare BSP 2.0.1 | Apache-2.0 | [派生文件及修改标记](firmware/main/board_port_waveshare_2_16.cpp)、[许可正文](third_party/moto-gps-waveshare/LICENSES/Waveshare-Apache-2.0.txt)、[来源记录](docs/BOARD_2_16_ADAPTATION.md) |
| Ducati 盾标与启动画面素材 | 品牌标识保持原权利状态；SVG 来源页标注 PD-textlogo / 商标限制 | [素材及来源记录](firmware/assets/boot/README.md)、[原始 SVG](firmware/assets/boot/ducati-red-logo.svg) |
| Source Han Sans SC 字体子集 | SIL OFL 1.1 | [许可正文](third_party/moto-gps-waveshare/LICENSES/SourceHanSansSC-OFL.txt) |
| Montserrat 字体 | SIL OFL 1.1 | [许可正文](third_party/moto-gps-waveshare/LICENSES/Montserrat-OFL.txt) |
| OpenStreetMap 地图数据、济南派生数据库及演示几何 | ODbL 1.0，© OpenStreetMap contributors | [原始署名](third_party/moto-gps-waveshare/shared/offline_map/ATTRIBUTION.txt)、[数据库和生成说明](third_party/moto-gps-waveshare/shared/offline_map/README.md) |

OSM 署名与许可入口：[OpenStreetMap copyright](https://www.openstreetmap.org/copyright)。
ODbL 正文：[Open Database License 1.0](https://opendatacommons.org/licenses/odbl/1-0/)。
上游完整派生数据库、生成说明及原始声明随快照保留。

LVGL 还包含各自标注许可的示例、字体和第三方实现，其文件级声明一并保留。
ESP-IDF、NimBLE、ESP-IDF 托管组件、Node 依赖、Emscripten / SDL 与其他开发依赖
仍按各自分发包中的许可证适用；构建得到的二进制所用材料应按实际依赖保留相应声明。
2.16 固件的实际组件版本与校验值见 [firmware/dependencies.lock](firmware/dependencies.lock)。
新增板级文件保留 Apache-2.0，其余新增集成代码和说明沿用根目录 PolyForm Noncommercial 1.0.0。
该许可不重新授权杜卡迪品牌标识或字体文件；启动文字仅保存预览所需的渲染图片，未附带 Arial 字体软件。

上游硬件资料和技术方案中的供应商参考文档、器件手册、符号、封装与参考图片
继续保留原发布者的版权及使用条件。根目录 PolyForm 许可证不重新授权这些材料。
详细范围和例外见保留的上游第三方声明。

高德、Apple Music、Apple MapKit 为外部服务 / SDK，其账号、服务数据和 SDK 使用权
不由本仓库的软件许可证授予。本次导入没有添加新的服务账号、签名证书或 API Key。
