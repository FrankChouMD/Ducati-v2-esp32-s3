# iOS 原生高德 SDK 接入

2026-10-16：Ducati 分支的实时导航已改为直接调用高德 iOS SDK，App 版本 0.4.6 (13)。
搜索、路线规划、手机路线地图、GPS 导航、语音和偏航重算不再依赖自己的域名或 Cloudflare。

## 构建与首次使用

在本仓库根目录执行：

```sh
python3 tools/setup_amap_ios_sdk.py
python3 tools/generate_ios_sdk_policy.py --check
xcodegen generate --spec third_party/moto-gps-waveshare/platforms/ios/project.yml
open third_party/moto-gps-waveshare/platforms/ios/MotoGPS.xcodeproj
```

下载脚本从高德官方地址获取固定版本并校验 SHA-256：导航（含地图）11.3.100、
Search 9.8.1、Foundation 无 IDFA 1.9.4，以及对应的官方隐私清单。
`Vendor/` 和 `Local.xcconfig` 已被 Git 忽略。安装脚本只在不存在时创建本地配置。

1. 编辑 `platforms/ios/Local.xcconfig`（位于上面的 third_party 项目目录内）：设置自己的
   `MOTO_APP_BUNDLE_ID` 和 `DEVELOPMENT_TEAM`。重新生成工程不会覆盖这些配置。
2. 在高德控制台创建或选用 **iOS 平台** Key，安全码 Bundle ID 必须与 App 完全一致。
   Web 服务 / JS Key 不适用。无需填写自己的网站。
3. 在 Xcode 选择自己的 iPhone，使用当前已登录的开发账号签名并 Run。
4. 首页打开“高德 SDK 设置”，填入 iOS Key，阅读隐私说明后自行选择同意并保存。
   Key 默认保存在本机钥匙串；SDK 在获得同意前不会初始化。
   `AMAP_IOS_KEY` 也可写入忽略的本地配置供个人构建预填，隐私同意仍需在 App 中完成。
5. 允许定位与蓝牙，搜索地点 → 选择终点 → 选择路线 → 开始导航，无需限摩确认。
   连接圆屏后，距离、转向、速度和简化路线沿用 BLE V1 下发；语音由 iPhone 播放。
6. “骑行设置 → 路线偏好 → 避开高架”默认开启，修改后点“保存”并重新规划。
   升级会保留原有路线偏好、区域和途经点；手动关闭该选项后会记住选择。

无签名 IPA 构建（需要之后重签）：

```sh
bash third_party/moto-gps-waveshare/scripts/ios/build_sideload_ipa.sh build/ios-sdk-sideload
```

此脚本生成不含个人 Key 的分发产物。使用任何重新签名工具时都要保留与高德 Key 匹配的
Bundle ID；若改变 Bundle ID，需使用为新 ID 配置的 Key。

## 规则与坐标

- 当前调用普通驾车导航引擎，根据设置选取不走高速 / 避免收费 / 躲避拥堵策略。
  [高德专用摩托车算路](https://lbs.amap.com/api/ios-navi-sdk/guide/route-plan/motorcycle-route-plan)
  需要独立开通；此版本未将普通驾车结果标为摩托车合法路线。
- 0.4.4 按用户要求移除自动限摩筛选：不检查南京区域、登记类型和桥隧限行名单，
  不再发送摩途避让多边形；区内起终点、途经点与穿区路线直接交给 SDK 规划。
  生成资源只包含坐标、用户路线偏好及几何检查，不再打包区域数据或限摩规则。
- 预览和导航页移除限摩评估卡、确认开关及区域覆盖图层；有有效预览即可点击开始。
  预览有效期和起点漂移检查继续防止使用过期路线。入口统一为“骑行设置”，不再选择登记类型。
- 0.4.2 增加独立的“避开高架”选项，按完整 Navi 路线每个 Link 的
  `AMapNaviLinkTypeElevatedRd`（4）筛选，含无名称的高架路段。
  普通桥梁（3）和主干道 / 城市快速路 roadClass（6）不直接判成高架。
  缺少道路类型或存在未知类型时不放行。判定依赖高德道路数据，仍需核对现场标志。
- 开启避开高架且初次候选全部冲突时，追加一次“不走快速路”策略（3）尝试，
  仍复查高速、轮渡、自定义区域和高架。这个策略只能产生候选，不能代替高架类型检查。
  如仍无可用路线，Search SDK 额外收到第一个已知高架路名作为 `avoidroad`，与区域约束同时使用。
- 普通导航候选全部不满足用户偏好时，调用 iOS Search SDK 的 `AMapDrivingV2RouteSearch`：
  只提交用户自行保存的避让区域（最多 4 个），保留途经点、高速、收费和轮渡偏好。
  Search SDK 结果通过完整自定义区域检查后，从道路坐标中选择引导点交给 Navi SDK 算路。
  Search 结果不含 Link 类型，只能作为绕行线索；最终 Navi 路线检查通过后才能用于导航。
  最多尝试两条绕行方案，每条依次使用总计不超过 8 / 16 个途经点；用户途经点保留原顺序。
  没有可用路线时提示具体偏好冲突，可在“骑行设置”调整。
- 一次规划期间保留各次独立算路的原生路线组，确保所选候选路线仍能启动导航。
  下一次规划开始时清理候选，并拒绝旧请求的迟到结果。
- GPS 重算不检查限摩区域及桥隧名单；继续检查用户设定的路线偏好。
  高架、用户区域等偏好不满足或路线数据不完整时，停止导航并提示重新规划。
  重算期间仍清除旧转向并暂停播报，接收新路线后恢复。
- CoreLocation / 用户设置使用 WGS84；送往 SDK 时转换一次为 GCJ-02。SDK 地图、导航回调和
  BLE 路线保持 GCJ-02，地点搜索结果逆变换后才存进现有 WGS84 数据模型。
- BLE 前方窗口最长约 1.5 km，按 24 点上限压缩；用户避让区域及高架检查始终使用完整路线。
  超过 5 秒或精度差的位置不用于更新实时转向、速度或路线位置。
- 0.4.6 在定位暂缺时继续发送当前路线最后一次有效的路线窗口、地图原点和行程进度，
  保留无有效定位标记。圆屏保留线路，顶部显示“等待定位”，暂时隐藏转向距离与限速，
  导航页速度显示 `--`；定位恢复后自动更新。缓存按路线 ID 和代次隔离，重算、到达、结束
  或切换路线后清除，BLE 重连时可重新发送。
- 从未获得有效路线窗口时，圆屏仍显示 `WAITING FOR GPS / 等待手机定位`；定位有效但
  路线窗口未到时显示 `SYNCING ROUTE / 正在获取路线`。只有没有活动目的地时才显示
  `READY TO RIDE`。手机定时更新使用主线程公共运行模式，滑动页面时继续发送状态。

## 底图和演示

手机路线预览使用高德地图。自定义区域编辑器和可选城市下载范围预览仍使用 Apple MapKit。
圆屏显示转向与路线不需要网关；周边 OSM 道路/建筑与离线包下载仍使用可选底图网关。
未配置时不会访问默认占位地址，已保存缓存和内置济南底图可继续使用。
SDK 地图瓦片不会转存或通过 BLE 分发给圆屏。

演示使用本地固定路线与原有共享 NavCore，不联系网关，也不验证真实 SDK 导航。
旧网关适配器保留供上游测试 / 历史实现使用，已不在实时搜索和导航调用链上。
旧 Web 网关不具备高架类型检查能力，会明确拒绝 `avoid_elevated_roads: true`；
旧响应缺少此字段时解码为未检查，不能匹配已开启避开高架的新设置。

## 验证

```sh
swift test --package-path third_party/moto-gps-waveshare/platforms/ios
node --test third_party/moto-gps-waveshare/backend/test/ios-sdk-policy.test.js
```

Xcode 的 `MotoGPSUnitTests` 增加了 JavaScriptCore 资源与 RoutePlan 解码、坐标往返、SDK
匹配 Link 的前方窗口、速度单位、BLE 点数限制、过期位置、偏航与到达后迟到回调测试。
厂商 framework 含真机 arm64 与模拟器 x86_64；Apple Silicon 模拟器需使用 Rosetta 目标并
设置 `EXCLUDED_ARCHS=arm64`，真机保持 arm64。不能将模拟器 / 构建通过当作真实道路验收。

官方接入依据：[手动部署](https://lbs.amap.com/api/ios-navi-sdk/guide/create-project/manual-configuration)、
[Search SDK 避让区域](https://a.amap.com/lbs/static/unzip/iOS_Map_Doc/AMap_iOS_API_Doc_Search/interface_a_map_driving_cal_route_search_request.html)、
[导航数据回调](https://lbs.amap.com/api/ios-navi-sdk/guide/navigation-map/navi-info)、
[道路类型与策略枚举](https://a.amap.com/lbs/static/unzip/iOS_Navi_Doc/_a_map_navi_common_obj_8h.html)、
[独立算路结果用于导航](https://lbs.amap.com/api/ios-navi-sdk/guide/route-plan/independentcalculateroute)、
[SDK 下载](https://lbs.amap.com/api/ios-navi-sdk/download/)。

### 0.4.0 初次接入验证（2026-10-16）

- Swift Package：20 项通过。
- iOS SDK 本地规则：8 项 Node 测试通过。
- iOS 17.4 x86_64 模拟器：56 项 App 单测通过（包含 6 项新增 SDK 测试）。
- iPhone arm64 Debug / Release 构建通过；0.4.0 (7) 无签名 IPA 结构和敏感配置校验通过。
- 使用当前 Apple 开发账号签名，成功安装并启动于连接的 iPhone 17 Pro Max。
- 设置页 UI 自动化在旧版 Rosetta 模拟器的无障碍快照阶段挂起，已停止，未计入通过项。
  尚未验证实际 SDK 搜索响应、GPS 导航、偏航、后台与 Cardo 语音。

个人开发构建已通过本机配置预填 Key；首次启用仍需用户在 App 内自行阅读并同意 SDK
隐私说明。源码与通用无签名 IPA 不包含个人 Key。

### 0.4.1 严格绕行修复验证（2026-10-16）

- 规则回归 33 项通过：包含 SDK 请求、28+4 区域坐标、端点冲突、完整路线复查及原网关规则。
- Swift Package 20 项通过；iOS 17.4 x86_64 App 单测 64 项通过。
- 新增测试覆盖：普通候选冲突后主动提交避让区域、8/16 点引导重试、仍穿区时拒绝、
  Search SDK 忽略避让时拒绝、途经点顺序、原生请求字段和不完整路线几何拒绝。
- 0.4.1 (8) Release 签名构建及签名校验通过，已安装到连接的 iPhone 17 Pro Max。
  已核对 App 内规则资源与源码一致。实际道路、账号避让区域权限及新绕行接口的在线响应仍待真机验证。
- 之前的 0.4.0 无签名 IPA 保持原版本；本次更新的是连接手机的个人开发构建。

### 0.4.2 高架避让验证（2026-10-16）

- 规则回归 39 项、协议 schema 检查 1 项、Swift Package 20 项、App 单测 73 项全部通过。
- 新增覆盖：无名称高架、普通桥梁与地面主干道区分、未知类型拒绝、完整 Link 遍历、
  高架候选的策略 / Search / Navi 重试、替代策略失败后继续绕行、取消后不继续请求、
  偏航高架拒绝、关闭高架避让仍保留其他限制、旧设置迁移与显式关闭持久化。
- 0.4.2 (9) Release 签名构建与签名校验通过，已安装至连接的 iPhone 17 Pro Max。
  App 内规则资源与生成源码一致。没有把模拟器测试和安装成功计作在线道路导航验收。
- 本次没有生成新版通用无签名 IPA；已有 0.4.0 无签名文件仍是原版本。

### 0.4.3 限摩提示与导航回退验证（2026-10-16）

- 规则与协议回归 45 项、Swift Package 20 项、App 单测 78 项全部通过。
- 覆盖区内起终点 / 途经点规划、可绕开时优先返回绕行路线、仍穿区时返回原生备选、
  Search 接口超时后回退、后续高架候选不能覆盖此前可用备选、取消后不返回旧备选、
  不带限摩多边形的补充搜索仍保留用户区域 / 途经点，以及偏航限摩提示不中断导航。
- 原生桥接测试验证独立算路组在同一次规划的重试间保留，备选可启动，下一次预览会清理。
  高架、高速、轮渡、自定义区域冲突和不完整道路数据仍被拒绝。
- 0.4.3 (10) Release 构建、签名与规则资源一致性校验通过，已安装到连接的 iPhone 17 Pro Max。
  实际在线搜索、路线与道路导航仍需真机验收。

### 0.4.4 移除自动限摩验证（2026-10-16）

- 42 项偏好 / 历史规则与协议回归、20 项 Swift Package 测试和 76 项 App 单测通过。
- 覆盖不同登记类型的区内起终点 / 途经点、穿区候选立即返回、桥隧名称不拦截、
  偏航穿区继续导航、无评估结果也可使用原生路线，以及自定义区域与道路偏好继续生效。
- 高架避让仍遵循独立开关；Search 只接收用户自定义区域，候选通过后不再执行限摩绕行重试。
- 生成资源不再包含摩途数据、端点限摩检查器或桥隧名单；签名 App 内资源与源码一致。
- 0.4.4 (11) Release 构建与签名校验通过，已安装至连接的 iPhone 17 Pro Max。
  本轮未运行在线 UI 流程或实际道路导航验收。

### 0.4.5 导航中误回 Ready 修复验证（2026-10-16）

- 复现原因：SDK 位置超过 5 秒未更新时会清空有效转向和路线窗口；旧圆屏仅根据路线点数
  判断导航界面，忽略仍存在的目的地，因此误显示 `READY TO RIDE`。
- 77 项 App 单测通过；新增覆盖位置过期、多次等待、原路线自动恢复以及主动结束导航。
- 真实 LVGL 渲染、NavPresenter 与 ESP32 BLE 桥接三个相关原生测试套件通过。
  渲染测试覆盖导航 → 等待定位超过 6 秒 → 等待路线 → 重算 → 恢复 → 结束，
  并检查局部刷新与完整重绘一致；已检查两种等待页面的渲染预览。
- 0.4.5 (12) Release 签名构建与签名校验通过，已安装并启动于连接的 iPhone 17 Pro Max。
- 配套 2.16 固件已使用 `app-flash` 更新、通过写入哈希校验；保留 NVS 与已有蓝牙配对。
  启动日志确认屏幕、触摸、BLE 与 QMI8658 初始化正常，并已与 iPhone 建立加密连接。
- 本轮验证覆盖构建、回归、安装及启动；尚未进行实际道路或锁屏持续导航验收。

### 0.4.6 无 GPS 时保留最后线路（2026-10-16）

- 79 项 App 单测通过，覆盖过期、精度不足及缺失定位时保留最后窗口和原点，恢复定位后
  更新窗口，以及路线 ID / 代次变更、重算、到达和结束时清除缓存。
- 真实 LVGL 渲染、NavPresenter 与 ESP32 BLE 桥接三个相关原生套件通过。检查无 GPS
  时地图区域像素保持一致、小提示不遮挡路线、页面切换后仍保留，以及 BLE 重连恢复。
  20 行局部缓冲渲染与完整重绘一致，已检查 `navigation-last-route-gps.ppm` 预览。
- 0.4.6 (13) Release 签名构建与签名校验通过，已安装并启动于连接的 iPhone 17 Pro Max。
- 配套圆屏固件已通过 `app-flash` 更新和写入哈希校验，启动及蓝牙连接正常。
  实际道路和锁屏持续定位仍待现场验证。
