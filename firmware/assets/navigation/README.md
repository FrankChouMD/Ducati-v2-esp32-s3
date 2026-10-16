# 导航当前位置俯视图标

2026-10-16，依据用户提供的红色 Ducati 摩托车俯视照片，通过内置 ImageGen 工具生成独立透明图标。
车头朝屏幕上方，固定显示在导航地图中心 `(240,240)`；右下角 E 版车尾图单独显示左右倾角。

## 文件与生成方式

- `ducati-top-source.png`：内置 ImageGen 返回的透明底原图，1024×1536。
- `manifest.json`：原图 SHA-256、裁切范围、导出尺寸与格式。
- `../../../tools/generate_navigation_icon.py`：只裁切透明边距、等比缩放、编码 LVGL 像素。
- `../../../third_party/moto-gps-waveshare/shared/nav_ui/assets/ducati_top.png`：48×112 运行时预览。
- 同目录的 `ducati_top.c` / `.h`：ARGB8888 常量，像素数据 21,504 字节。

在仓库根目录使用安装有 Pillow 的 Python 运行 `python tools/generate_navigation_icon.py` 可重建。
正常固件构建直接编译 C 常量，不依赖 Python 或在线生成服务。

该素材是本项目的生成示意图；参考照片和品牌权利仍归原权利人，未将参考照片复制到运行时资源。

## 最终提示词

生成方式：内置 `image_gen` 工具；参考图为本次会话用户提供的俯视照片；`transparent_background=true`。

> Use case: stylized-concept. Asset type: transparent navigation marker bitmap for a 480x480 motorcycle display. Reference image: the attached overhead red Ducati motorcycle photograph, used for body shape and red/black coloring. Create ONE clean isolated true orthographic top-down red sport motorcycle, no rider, front wheel and nose pointing straight UP (12 o'clock), rear pointing down, perfectly centered and vertically aligned, symmetrical mirrors/handlebars. Preserve recognizable red sculpted fuel tank, black saddle, red rear tail, dark windshield, black tires, small silver fuel cap. Simplified polished realistic 3D icon with strong readable silhouette and restrained highlights, legible when reduced to approximately 50x80 pixels. Whole motorcycle including mirrors and both tires fully inside frame, modest transparent padding. Actual transparent background, no ground, no shadow outside the motorcycle, no map, no labels, no border, no arrows, no text, no watermark. This is a new asset inspired by the reference, not the full photograph. Portrait orientation of motorcycle.
