# Windows 本机测试

在项目根目录双击 **[run-local.cmd](../run-local.cmd)**，会编译并打开本机 LVGL 模拟器。
当前工作区已经构建好，也可以直接运行：

```powershell
.\build\native-480\Release\ducati_preview.exe
```

模拟器直接复用固件中的 `ducati_boot_animation.cpp`、生成素材及 MOTO GPS 的共享 UI。
窗口先播放 2.4 秒开机动画，随后自动进入使用模拟数据的导航界面。
480×480 使用方屏布局：导航底部为水平路线进度条，分页提示位于其下方。
仪表采用杜卡迪参考图的黑 / 白 / 红配色和窄体斜体读数，实际导出预览见 [仪表样式](INSTRUMENT_STYLE.md)。
导航页左上为方向、右上为限速、左下为速度表、右下为倾角，底部正中为 E 版摩托车。
摩托车后轮接地点与路线的当前位置原点对齐。演示车速在约 58±8 km/h 之间变化。
导航页右下角、速度页下方显示模拟倾角，随时间在左右约 32° 之间变化；
`L` 为左倾、`R` 为右倾，选定的 E 版车尾图同步左右倾斜，虚线为直立参考；暂停会同时冻结数值和图形。
不会连接手机、车机、地图服务或蓝牙设备。

| 操作 | 按键 / 按钮 |
| --- | --- |
| 重播开机动画 | R / 重播 |
| 暂停或继续动画、演示数据 | 空格 / 暂停或继续 |
| 提前进入导航演示 | N / 导航演示 |
| 查看导航、速度、罗盘 | 1、2、3 |
| 切换上一页 / 下一页 | 左右方向键 |

构建需要 Visual Studio 2022 的 C++ 桌面开发工具和 Windows SDK。
`run-local.cmd` 优先使用 PATH 中的 CMake，也兼容当前电脑上的 VS 2022 Community 内置 CMake。
它只构建本机目标，不会触碰开发板或烧录 Flash。

## 自动检查

只编译并运行模拟器的无窗口检查：

```powershell
.\run-local.cmd --check
```

这会检查动画切入导航、三页切换、暂停，以及在导航中重播动画的控件和定时器生命周期。
运行工程全部桌面检查：

```powershell
cmake --build build/native-480 --config Release --parallel 1 -- /nodeReuse:false
ctest --test-dir build/native-480 -C Release --output-on-failure
```

当前 Windows / 480×480 下 **12 项检查全部通过**，包含倾角估算、传递与页面显示。
模拟器程序已编译，
无窗口自检已通过；实际桌面窗口的点击和显示效果由使用者进一步检查。

需要导出真实 LVGL 页面像素时，可在输出目录运行模拟器的 `--capture` 参数。
它不会打开窗口，会在当前目录生成 `navigation.ppm`、`speed.ppm` 和 `compass.ppm`；
可用支持 PPM 的图片工具查看。导出会直接显示静止页面，跳过连接过渡动画。
还会导出两个倾角页面的 `right-30`、`left-30`、`upright`、`unavailable`、`right-85`
状态，用于检查图形方向、扶正、大角度和数据失效的显示。
导航页另导出 `navigation-approved`（58 km/h、扶正）、`navigation-high-speed`（160 km/h）、
`navigation-no-limit`（限速缺失）和 `navigation-km`（12.3 km）状态。

## 范围

可以检查启动动效、真实 LVGL 的界面绘制、演示页面和共享导航 / 协议逻辑。
导航演示使用构造的路线与速度，不是高德在线算路结果。
模拟器倾角为演示数据；QMI8658 真机估算和长按校准说明见 [LEAN_ANGLE.md](LEAN_ANGLE.md)。
真实触摸、屏幕 DMA / 刷新、QMI8658、AXP2101、BLE 与 iPhone / Cardo 连接仍需接板测试。
桌面机器的速度和内存也不能代表 ESP32 的硬件性能。

只想查看已确认的设计效果，也可直接打开
[浏览器动画预览](previews/ducati-panigale-v2s-boot.html)。
