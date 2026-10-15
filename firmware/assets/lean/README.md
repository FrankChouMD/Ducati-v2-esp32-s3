# E 版车尾倾角图标

2026-10-15，用户从 A–F 六版方案中选定 E 版，用于导航页与速度页的倾角显示。
图形依据本次会话提供的红色跑车车尾照片生成，保留红色尾部、后视镜、双排气和宽后胎。

## 来源

- `design-options.png`：本次会话通过 ImageGen 生成的六版比较图，E 位于下排中央。
- `ducati-rear-e-source.png`：通过 ImageGen 从方案图提取、整理的 E 版透明底图，1024×1536。
- `manifest.json`：源文件 SHA-256、透明边距裁切范围、导出尺寸、接地点和生成结果校验值。

该素材为本项目的生成示意图，Ducati、Panigale 等品牌名称与相关权利仍归原权利人。
新增生成器、整合代码与文档适用根目录 PolyForm Noncommercial 许可；不据此重新授权品牌或参考照片。

## 运行时资源

`tools/generate_lean_icon.py` 仅裁切透明边距、等比缩放和转换像素格式，保留选定图形。
导出到 `third_party/moto-gps-waveshare/shared/nav_ui/assets/`：

- `ducati_rear_e.png`：86×128 透明预览。
- `ducati_rear_e.c` / `.h`：LVGL ARGB8888 常量与尺寸、后轮接地点定义，像素共 44,032 字节。

正常构建直接使用 C 数据。LVGL 在两个页面按同一倾角旋转图标，固定缩放保证左右 90° 的包络留在图形区域内。
数值失效时隐藏摩托车，显示灰色叉号；固定的直立参考线和地面线保持原位。
导航页将车尾图置于底部正中，后轮接地点与地图原点对齐；右下角单独显示倾角与参考刻度。
导航页倾角失效时保留位置点，速度页继续使用原有的虚线 / 地面参考。

需要重建资源时，在仓库根目录运行：

```powershell
python -m pip install Pillow
python tools/generate_lean_icon.py
```
