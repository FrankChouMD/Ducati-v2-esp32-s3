# Waveshare 2.16 固件

目标板为 **ESP32-S3-Touch-AMOLED-2.16**：480×480 AMOLED、16MB Flash、8MB PSRAM。
这是本项目的固件入口，复用已导入的 MOTO GPS 导航、BLE 协议和 iPhone 配套代码。
上游的 `platforms/esp32` 入口仍面向 1.75C，烧录本板时请使用本目录。

## 环境和构建

使用 ESP-IDF **5.5.5**，目标芯片为 `esp32s3`。首次构建需要联网下载组件。
主要依赖固定在 [main/idf_component.yml](main/idf_component.yml)，间接依赖及校验值在
[dependencies.lock](dependencies.lock)。LVGL 使用仓库内已展开的 9.5.0 源码。

在 ESP-IDF 命令行中，进入本仓库根目录后运行：

```powershell
idf.py -C firmware build
```

构建产物为 `firmware/build/ducati_v2_amoled_2_16.bin`；同时生成 bootloader、分区表和烧录参数。
如更改 Flash / PSRAM 设置，应删除本目录生成的 `sdkconfig` 后重新构建，使
[sdkconfig.defaults](sdkconfig.defaults) 生效；保留自己的设置时可通过 `menuconfig` 调整。

本次验证使用的隔离工具链位于仓库的忽略目录 `build/`。在当前 Windows 工作区可这样激活：

```powershell
$env:IDF_TOOLS_PATH = Join-Path (Get-Location) 'build/esp-idf-tools'
. ./build/esp-idf/export.ps1
idf.py -C firmware build
```

该工具链未纳入 Git。其他电脑请先自行安装和激活 ESP-IDF 5.5.5。
CMake 在依赖解析前设置 `DUCATI_LVGL_PATH`，并将锁文件中的本地 LVGL 路径保存为环境变量引用，
避免把当前电脑的盘符和工作目录固定进依赖清单。

## 烧录和串口

接上开发板，确认串口号后，在仓库根目录运行（将 `COM7` 换成实际串口）：

```powershell
idf.py -C firmware -p COM7 flash monitor
```

这里记录操作方法，本次没有刷写设备。首次烧录会写入本项目的 bootloader、分区表和应用。
分区表使用 8MiB factory 应用区，剩余空间预留为 SPIFFS storage；当前没有 OTA 与骑行日志写入。

## 桌面检查

Windows 可在仓库根目录双击 [run-local.cmd](../run-local.cmd)，
观看真实 LVGL 开机动画和导航界面演示，并使用重播、暂停及页面切换。
操作方法和检查范围见 [LOCAL_TESTING.md](../docs/LOCAL_TESTING.md)。

根目录 CMake 默认按 480×480 编译真实 LVGL UI、导航逻辑和 BLE 协议测试。
在安装了 Visual Studio 2022 C++ 工具的 Windows 终端中运行：

```powershell
cmake -S . -B build/native-480 -G "Visual Studio 17 2022" -A x64 -DDUCATI_CANVAS_SIZE=480
cmake --build build/native-480 --config Release --parallel 1 -- /nodeReuse:false
ctest --test-dir build/native-480 -C Release --output-on-failure
```

其他系统可用本机的 CMake 生成器构建。`DUCATI_CANVAS_SIZE=466` 可检查上游默认尺寸。
桌面渲染测试使用主机内存和模拟 flush，不能证明实际 PSRAM 分配、DMA 时序或触摸方向正确。

## 当前行为

- 启动显示已确认的杜卡迪盾标动画，**PANIGALE V2S** 逐字渐入；2.4 秒结束后进入共享导航 UI。
- CO5300 使用 480×480 地址范围和零像素偏移，首次黑帧绘制后才点亮屏幕。
- 方屏导航使用底部水平路线进度条，分页提示在其下方；与本机模拟器共用实现。
- 触摸采用官方 2.16 BSP 的 CST9220 兼容驱动和坐标方向。
- 电源键使用 GPIO16 的高电平；保留上游长按关机与深睡眠兜底行为。
- QMI8658 同时提供航向辅助与倾角原型估算，使用手机速度补偿转弯运动加速度。
  导航页与速度页显示左右倾角及同步倾斜的 E 版车尾图；扶正静止约 2 秒建立零点，长按倾角区域可重新校准。
  未校准或数据失效显示 `--°`。安装轴向、假设与验证范围见 [LEAN_ANGLE.md](../docs/LEAN_ANGLE.md)。
  尚未保存或同步倾角日志。
- BLE 继续使用上游数据协议，未启动音频服务。iPhone 与 Cardo 直接连接的方案仍需组合实测。

板级引脚、来源、修改文件和接板验收项目见
[BOARD_2_16_ADAPTATION.md](../docs/BOARD_2_16_ADAPTATION.md)。
动画预览、实现和测试见 [BOOT_ANIMATION.md](../docs/BOOT_ANIMATION.md)。
iPhone App 的签名与安装见 [上游 iOS 说明](../third_party/moto-gps-waveshare/platforms/ios/README.md)。
导航服务沿用上游高德普通驾车路线，尚未实现摩托车算路或 CarPlay 接入。

原始作者和许可证见 [NOTICE](../NOTICE) 与 [第三方许可索引](../THIRD_PARTY_NOTICES.md)。
