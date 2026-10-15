# 杜卡迪风格仪表字体

仪表使用 Barlow Condensed 1.408，版权归 The Barlow Project Authors。
字体源文件与生成的 LVGL 子集均适用本目录 [SIL OFL 1.1](OFL.txt)，保留原始版权和许可，
不适用本仓库代码的非商业许可。

## 来源与生成

源文件来自 [Google Fonts 的 Barlow Condensed](https://github.com/google/fonts/tree/89f5431ff0db41bd2fe3f7ba21a723a01622428b/ofl/barlowcondensed)：

- `BarlowCondensed-Medium.ttf`：18 / 22px 标签与单位。
- `BarlowCondensed-SemiBoldItalic.ttf`：32px 速度页倾角 / 长距离 / 状态，56px 导航页车速 / 倾角 / 距离与航向，88px 速度页读数。
- `OFL.txt`：字体原始许可全文。

这是按参考仪表的窄体、斜体风格选择的替代字型。仪表效果见 [设计说明](../../../docs/INSTRUMENT_STYLE.md)。
已生成的字库位于 `third_party/moto-gps-waveshare/shared/nav_ui/assets/fonts/`，正常构建不需要 Node.js 或字体转换器。
字库使用 4bpp 灰度抗锯齿；大号读数打包数字、标点和度数符号，56px 另包含倾角方向 `L` / `R`，其他缺字回退到既有中文子集。
`manifest.json` 记录原始字体、许可和生成结果的 SHA-256。

需要重建时，在仓库根目录运行：

```powershell
npm.cmd install --prefix build/font-tools --cache build/npm-cache --ignore-scripts --no-audit --no-fund --save-exact lv_font_conv@1.5.3
python tools/generate_instrument_fonts.py
```

转换工具为 [LVGL 官方 lv_font_conv](https://github.com/lvgl/lv_font_conv)，固定 1.5.3；生成脚本会核对工具版本。
重新分发字体、生成子集或固件时保留本目录的版权与许可入口。
