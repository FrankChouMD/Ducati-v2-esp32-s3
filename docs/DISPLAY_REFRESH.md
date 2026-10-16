# 倾角刷新与独立定位图标

2026-10-16，ESP32-S3-Touch-AMOLED-2.16，480×480 / 40MHz QSPI。

## 修改

- 导航当前位置使用屏幕中心 `(240,240)` 的 48×112 俯视摩托车，车头朝上。
  路线、道路与建筑仍使用同一个地图投影原点。倾角变化或失效都不改变定位图标。
- 选定的 E 版车尾图移到右下倾角面板，与数字使用同一份读数；速度页保留原有布局。
- 纯倾角刷新跳过地图投影和无关控件，清理整个局部面板后立即提交，避免旧文字或车尾图残留。
  航向发生变化时仍更新可见地图或罗盘；隐藏页面不因航向变化重投影。
- 保留 25ms / 40Hz 请求节拍；加速度与陀螺仪使用相同的 20ms 预滤波时间常数（原为 35ms）。
- 内部 RAM 单绘图缓冲由 10 行增至 20 行，降低分片开销。PSRAM 直接 DMA 仍关闭，
  LVGL 的额外 2MiB 分配池仍在 PSRAM。

素材来源及提示词见 [navigation/README.md](../firmware/assets/navigation/README.md)。
当前真实 LVGL 页面预览见 [INSTRUMENT_STYLE.md](INSTRUMENT_STYLE.md)。

## 板载测量

同一新版布局、同一 20 行缓冲，每种模式连续 40 帧，倾角按正弦轨迹变化。
比较强制整屏重绘和只更新倾角面板。BLE 保持运行且 iPhone 已连接；测试期间持有显示锁，
避免手机状态覆盖固定测试页面。每帧计时包含设置倾角与 `lv_refr_now` 的绘制 / 提交耗时，
不包含主动让出 CPU 的间隔、IMU 采样与滤波，也不代表完整导航场景的稳定帧率。

| 页面 / 方式 | 平均耗时 | P95 耗时 | 平均传输像素 / 帧 |
| --- | ---: | ---: | ---: |
| 导航 / 整屏 | 85.447ms | 86.676ms | 230,400 |
| 导航 / 局部倾角 | 10.983ms | 11.464ms | 23,068 |
| 速度 / 整屏 | 127.400ms | 128.217ms | 230,400 |
| 速度 / 局部倾角 | 18.734ms | 19.439ms | 26,250 |

同场景平均耗时分别缩短约 87% 和 85%；像素传输减少约 90% 和 89%。
测试结束时内部可用内存为 26,223 字节。启动与测试日志未出现 DMA underflow、分配失败或崩溃，
QMI8658 随后初始化并完成静止校准。实车动态精度、主观流畅度与复杂地图组合仍需实际体验。

本工作区的原始日志：`build/refresh-benchmark-isolated-boot.log`（忽略目录）。
可选测试源码为 `tools/display_refresh_benchmark.cpp`，正常构建默认不包含该代码。

正常版本已以 `DUCATI_DISPLAY_BENCHMARK=OFF` 重新构建、写入 `/dev/cu.usbmodem1101` 并完成 Flash hash 校验。
应用大小 1,554,208 字节，SHA-256 为
`8defe10d224e398a622c7f984a71a66cb1aa1d0d0c8e044da7b2c6727d6e2bea`。
正常启动日志确认 BLE、QMI8658 及 `Lean calibration ready`，未执行基准测试。
烧录与启动记录分别为 `build/refresh-final-flash.log`、`build/refresh-final-boot.log`。

激活 ESP-IDF 后，在仓库根目录运行（替换实际串口）：

```sh
idf.py -C firmware -DDUCATI_DISPLAY_BENCHMARK=ON -p /dev/cu.usbmodem1101 flash monitor
```

该模式会在启动时暂时切换测试页面。测试完必须恢复正常固件：

```sh
idf.py -C firmware -DDUCATI_DISPLAY_BENCHMARK=OFF -p /dev/cu.usbmodem1101 flash
```

## 回归检查

macOS / 480×480 的四项相关检查全部通过：`motion_lean_fusion_tests`、`phone_nav_bridge_tests`、
`nav_presenter_tests`、`nav_ui_render_tests`。渲染测试另以与固件一致的 20 行单缓冲运行通过。

检查包含：左右 85°、30° 和 0° 的局部清理，两页局部更新像素数低于屏幕四分之一，
中央图标不因倾角改变，失效时仍显示定位图标，纯倾角映射保留已有路线及字符串。
局部与完整重绘比较允许最多 32 个像素出现单个 RGB565 色阶的差异，以容纳 LVGL 软件缩放
对不同裁切原点的舍入；不允许旧图形或文字残留。
