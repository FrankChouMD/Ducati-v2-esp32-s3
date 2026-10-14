# MOTO GPS 导入记录

导入日期：2026-10-14（Asia/Singapore）。用途：杜卡迪 V2 导航车机个人非商业自用开发。

## 来源与固定版本

| 材料 | 来源 | 固定提交 |
| --- | --- | --- |
| MOTO GPS · Waveshare Edition | [Maler X / moto-gps-waveshare](https://github.com/mx3353672833-debug/moto-gps-waveshare) | `c8bdacca0e01dda85260975aa2027c1ae8a549e5` |
| LVGL 9.5.0 | [lvgl/lvgl](https://github.com/lvgl/lvgl) | `85aa60d18b3d5e5588d7b247abf90198f07c8a63` |

MOTO GPS 使用 PolyForm Noncommercial 1.0.0；LVGL 使用 MIT。
LVGL 的提交直接取自上游 Git 子模块记录，没有改用当前最新分支。

## 导入方式

上游完整文件树保存在 `third_party/moto-gps-waveshare/`。
原来的 `third_party/lvgl` 子模块在此目录内展开为普通文件，方便单仓库保存和获取。
保留上游 `.gitmodules` 供追溯，当前仓库无需再次初始化该子模块。

文件直接读取自上述提交的 Git blob，避免 Windows 换行转换、Git checkout 过滤器
或导出处理改变许可证、文本、生成资源、数据库和硬件档案的字节。
上游文件内容没有修改；原作者 Logo、署名、注释、许可例外和文档均保留。
上游和 LVGL 的独立 `.git` 元数据不进入导入目录。

上游原本强制纳入版本控制的 166 个文件会被其继承的忽略规则隐藏，包括硬件归档、
LVGL 的 ThorVG 配置和测试用二进制资源。集成时在对应目录新增精确白名单 `.gitignore`，
原有 `.gitignore` 内容仍保持不变；只开放清单中的既有文件，新增构建产物继续忽略。
这些附加文件单独记录在来源清单的 `integration_added_files` 中。

新增的集成文件位于当前仓库根目录和 `docs/`；根目录 `LICENSE.md` 与上游
`LICENSE.md` 字节一致。根目录 `NOTICE` 再次列出上游要求的两行 `Required Notice:`，
完整的上游 `NOTICE` 仍保留在快照中。

## 可核对的清单

[third_party/moto-gps-waveshare.upstream.json](../third_party/moto-gps-waveshare.upstream.json)
记录每个文件的来源仓库、原始路径、Git blob SHA-1、SHA-256、字节数和原始 Git 文件模式。

导入时进行了两项校验：

1. 根据文件内容重新计算 Git blob SHA-1，与固定提交的文件树逐项比对。
2. 写入后重新读取每个文件，计算 SHA-256，与导入清单逐项比对。

已校验 **5665 个上游文件**，原始内容共 **287874980 字节**。
根目录许可正文与上游一致，两行必需署名保留；入口文档的本地链接有效，
全部上游文件均可被当前 Git 仓库识别，不含独立 `.git` 目录。

Windows 文件系统不表达 POSIX 可执行位；原始 `100755` 模式已记录在清单中，
并在导入 commit 中恢复为对应的 Git 文件模式。macOS / Linux 检出后可按原模式运行；
在 Windows 上可按脚本类型显式使用 `bash` 或 `python` 运行。

## 当前功能边界

本次工作为源码与许可材料导入，不包含板级移植或功能改写。
上游适配的是 **ESP32-S3-Touch-AMOLED-1.75C**，不是用户选定的 **2.16** 板子。
后续需核对 CO5300 初始化、触摸控制器、480×480 布局、16MB Flash 分区及电源 / IMU 接口。

上游目前使用高德普通驾车路线接口；摩托车禁限行算路需要后续接入。
倾角记录、SD 数据格式、iPhone 回放及 Cardo 组合实测仍需后续完成。
上游已有 iOS 锁屏 / 后台 BLE 恢复问题，详见
[KNOWN_ISSUES.md](../third_party/moto-gps-waveshare/docs/KNOWN_ISSUES.md)。

本次未进行 ESP-IDF 或 Xcode 编译、刷机、手机连接或实车测试。

## 后续修改与分发

修改上游文件时，应保留其原始版权和文件级许可证，并明确记录改动。
不要给 MIT、Apache-2.0、OFL 或 ODbL 材料改贴 PolyForm 非商业标签。
复制或分发项目时，应一并携带许可正文 / 链接与上游必需署名。
源码许可不替代高德等外部服务、供应商参考材料或构建依赖的授权条件。
