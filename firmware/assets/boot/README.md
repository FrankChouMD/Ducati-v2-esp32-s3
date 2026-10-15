# Ducati / PANIGALE V2S 启动素材

本目录保存 2026-10-15 确认的启动动画素材，目标画布为 480×480。
确认的浏览器预览保存在 [docs/previews/ducati-panigale-v2s-boot.html](../../../docs/previews/ducati-panigale-v2s-boot.html)。

## 来源与权利声明

- `ducati-red-logo.svg`：Ducati Motor Holding 的盾标，矢量化者为 ReneeWrites。
  来源为 [Wikimedia Commons 的文件页](https://commons.wikimedia.org/wiki/File:Ducati_red_logo.svg)，
  文件地址为 [原始 SVG](https://upload.wikimedia.org/wikipedia/commons/3/36/Ducati_red_logo.svg)。
  来源页标注为 `PD-textlogo`，同时标注商标限制；Ducati 标识和品牌权利仍归原权利人。
- 固件中的盾牌底图、独立字标和轮廓点由该 SVG 生成；保留以上来源，不将品牌素材改贴为本项目原创。
- `PANIGALE V2S` 与确认预览一致，使用本机 Arial Bold Italic 27px、2.5px 字距栅格化为 11 张字母图片。
  仓库不附带 Arial 字体文件或可安装的字体软件。生成时使用的字体文件 hash 记录在 `manifest.json`。
- `ducati_boot_assets.c` / `.h` 包含图片和轮廓数据；品牌素材保持其原始权利状态。
  生成器和动画程序的本项目代码适用根目录许可，不能据此重新授权第三方标识。

## 固件资源

像素素材共 **189,364 字节**，为小尺寸 ARGB8888 常量；另外保存 478 个轮廓采样点。
运行时由 LVGL 绘制，无需 SVG 解析器、视频解码器或整个屏幕的逐帧图片。
模糊光晕在设备实现中使用三层半透明轮廓近似，其余位置和渐入时序沿用确认预览。

动画由单个 25ms LVGL 定时器驱动，总时长 2400ms：

| 效果 | 时间 |
| --- | --- |
| 红色轮廓描边 | 70–670ms |
| 盾标底图渐入 | 490–940ms |
| DUCATI 字标渐入 | 650–1060ms |
| PANIGALE V2S 逐字渐入 | 850ms 开始，间隔 65ms，每字渐入 210ms |
| 整体淡出 | 2060–2400ms |

画面起止均为纯黑；完整文字出现后保留短暂停留，再淡出进入导航。
切换到导航界面或重新开始动画时，删除事件会取消原来的定时器。

## 重新生成

已生成的 C 文件直接参与编译，正常构建不需要 Python 图形依赖。
需要修改品牌素材或文字时，在仓库根目录运行：

```powershell
python -m pip install Pillow PyMuPDF fonttools
python tools/generate_boot_assets.py --font C:/Windows/Fonts/arialbi.ttf
```

在其他系统上，`--font` 应指向有权使用的同一字体；换字体会改变最终效果。
生成器从 SVG 和本机字体重建 C 数据与清单，清单记录来源和生成结果的 SHA-256。

验证使用 [tests/boot_animation_render_tests.cpp](../../../tests/boot_animation_render_tests.cpp)
渲染固件的实际 LVGL 程序，检查黑帧、文字先后顺序、重复启动及导航接管。
