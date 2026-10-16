# 南京禁外摩边界检索与核验

核对日期：2026-10-16。适用车辆按用户确认的南京郊区登记号牌处理。

## 结果

已找到 **政府原始附图**，以及摩途官网公开地图使用的 **124 点南京区域候选**。
GitHub 检索和已检查仓库中，未找到同时具有南京精确几何、政策对应关系、
坐标系声明及更新记录的可直接采用数据集。这个结论仅覆盖本次检索范围。

还已取得 OpenStreetMap 固定快照，提取实际道路、水系线形并进行了坐标偏移比较。
用户确认后，已采用摩途公开图层作为第三方辅助基线，启用区域避让、穿区复查和手机图层显示。
原始边界保留 124 点，派生为 28 个符合上游限制的分区，构建校验并集与原始区域一致。
仍未进行真实高德路线或实车验证，几何有效不等同于政策边界准确。
当前实现和验证见[南京骑行说明](../NANJING_MOTORCYCLE_NAVIGATION.md)。

## 官方依据与原图

1. [市政府关于外牌摩托车禁行区域的通告](https://www.nanjing.gov.cn/xxgkn/szgfxwj/202311/t20231115_4098444.html)。
   正文落款 2007-03-07，行政规范性文件库页面保存了原始附图。
   条文将江宁、浦口、六合、溧水、高淳登记号牌纳入所述外牌类别，涉及长江以南、
   绕城公路以内及南京长江大桥。2023 年 URL 不能当成政策新颁布日期。
2. [公安局对第 0033 号人大建议的答复](https://www.nanjing.gov.cn/xxgkn/jytabljggk/2025njytabl/shirddbjy/202512/t20251205_5705995.html)。
   正文落款 2025-07-31、网页发布 2025-12-05；再次描述该范围，并记载当时通告到期日为
   2027-07-31。该日期需要随之后的延续、修订文件重新核对，不能自动当成永久有效。
3. [玄武区 2025 年第 146 号行政复议决定书](https://www.xwzf.gov.cn/xwqrmzf/202508/t20250806_5623248.html)。
   2025-08-06 发布，具体说明墨香路相关路段在禁行范围内、进城标志辅标涉及郊区登记号牌。
   这是解释适用对象和验证已知区内样本的依据，不是全区域坐标清单。
4. [2025 年度规范性文件备案目录](https://rd.nanjing.gov.cn/gfxwjbasc_0/202604/t20260423_5828877.html)。
   目录第 15 项列有同名通告；目录没有坐标或新版全文，不能仅据目录认定边界已经变更。

[原图在线地址](https://www.nanjing.gov.cn/xxgkn/szgfxwj/202311/W020231115543830929366.jpg)
及[未经修改的本地核对副本](nanjing-2007-official-map.jpg)。
该图用红色虚线标出扩大后的范围，另有原范围图例；**没有地理坐标、比例尺或投影说明**，
不宜直接按图片像素拉伸配准成精确导航围栏。

原图 SHA-256：`235c8c922257ff8636c09e9ffab710f90858efb37abe49d56a7bcca4f410f8af`。
附图保持原发布者权利状态；仓库软件许可证不重新授权该材料。

### 绕城公路的定位依据

[交通运输局 2025-11-05 工程公告](https://jtj.nanjing.gov.cn/njsjtysj/202511/t20251105_5683755.html)
明确了旧绕城公路相关路段：G36 东杨坊至马群、G42 马群至刘村。
这是核对道路身份的资料，不是完整禁外摩边界定义，尤其不能拿外围 G2503 绕越高速替代。
河岸、立交、桥头接线、平行辅道及边界道路是否纳入仍需独立核对。

## 摩途公开坐标候选

入口为[摩途官网地图](https://www.motumap.com/)，页面直接加载
[`nodata.js`](https://www.motumap.com/nodata.js)，由
[`prohibited_area_map.js`](https://www.motumap.com/prohibited_area_map.js) 绘制。
本次只读取公开静态资源，没有登录、提交车牌、调用受限接口或执行其代码。

| 项目 | 实际读取及检查结果 |
| --- | --- |
| 南京记录 | `CityID=025`、`AreaID=10255`，南京记录 1 条 |
| 数据名称 | 南京绕城高速内限外埠 |
| 几何 | 124 个互异顶点，`IsArea=1`；原始首尾未重复，按多边形规则闭合 |
| 范围包络 | 经度 118.624092–118.898930；纬度 31.936582–32.155948，仅描述候选范围 |
| 基础几何 | 检查所有非相邻边，包括闭合边，未检出自相交 |
| 粗略面积 | 约 288.27 平方公里，采用纬度 32 度的局部平面近似计算 |
| 坐标系 | **推断为 GCJ-02**：绘制代码把坐标直接交给高德 Polygon；数据未显式声明 |
| 记录字段 | 有号牌类别等字段，没有逐条政策来源、核验时间、有效期或几何精度 |
| HTTP Last-Modified | 2025-12-25 07:22:12 GMT，只代表静态文件响应元数据 |
| 文件 SHA-256 | `1a98ca7f61835a3cff9e4f39d72eb03d5119a0d5fa56cab2ad3b224ee5617c1c` |

基础几何检查没有证明河岸、匝道及辅道的归属。记录名称和其号牌筛选代码也不能替代
本项目明确保存的 `nanjing_suburban` 登记类别。HTTP 时间不是南京记录的核验日期。

[摩途服务条款](https://motu.motumap.com/service-policy.html) 对服务内容列有知识产权和限行数据说明；
本次未找到独立开放数据许可证或南京区域导出文档，不能把网页可查看等同于开源授权。
完整官网抓取快照保存在 Git 忽略的 `build/nanjing-boundary-research/`，
从中选择的南京区域及派生分区已保存到共享运行数据，由网关用于检查并返回手机显示。

用户随后接受以摩途作为第三方辅助数据源，接入方向以该图层为基线。
另已核对其[电子眼数据入口](MOTU_CAMERA_DATA.md)：包含“闯限行”等类别，但摄像头走独立接口，
本次网页入口未取得真实南京点位；不能用脚本的随机模拟点代替。

## 已取得的 OpenStreetMap 物理参考线

已从 [Geofabrik 江苏下载页](https://download.geofabrik.de/asia/china/jiangsu.html)取得固定快照
`jiangsu-261014.osm.pbf`，并与发布的 MD5 校验文件比对通过。
原始大文件保存在本机研究目录。数据时间为 **2026-10-14T20:21:12Z**，
MD5 为 `d51391c1fbd7f6ed824754593abfdc70`。

项目中保存了 [WGS84 物理参考线 GeoJSON](nanjing-osm-reference.geojson) 和
[提取脚本](../../tools/extract_nanjing_osm_reference.py)，含 **284 条道路 way、11 条长江／夹江水系 way**。
每条保留 OSM way ID、版本、时间、相关原始标签，整体保留来源及 ODbL 署名。
这些是物理线形，**不是禁行区域**；水系 way 可能为河道中心线，不保证是南岸岸线。
保留外围高速作对照，不应把文件内所有道路合围成禁行区。筛选包络也不作为围栏。

对摩途候选作了坐标假设比较：在假设 GCJ-02 转 WGS84 后距提取道路不超过 200 米的
95 个顶点样本中，转换前距道路中位数约 293 米，转换后约 35 米。
样本按转换后的距离筛选，**这不是独立的坐标系或位置精度验证**；
它支持 GCJ-02 的推断，也说明仍有道路贴合误差，不能据此称为精确边界。
计算结果保存在本机 `build/nanjing-boundary-research/road-alignment-audit.json`。

复现时安装 `osmium==4.3.1`，下载同名 PBF 及 `.md5`，在仓库根目录运行：

```powershell
python tools/extract_nanjing_osm_reference.py --input build/nanjing-boundary-research/jiangsu-261014.osm.pbf --checksum build/nanjing-boundary-research/jiangsu-261014.osm.pbf.md5 --output docs/research/nanjing-osm-reference.geojson
```

## GitHub 检查记录

通过仓库搜索、文件搜索及实际文件检查，排除了以下容易被名称误导的候选。

| 仓库 / 已检查入口 | 结果 |
| --- | --- |
| [CandyCloudCC/jinmo-policy-hub](https://github.com/CandyCloudCC/jinmo-policy-hub/tree/43966dfbc7f06d0eaa2b5ccb93a59fbc08dbf428) | 法规及案例资料库，有南京对话文字；目录中没有南京矢量边界 |
| [z2629321393-ai/changsha-moto-map/data/zones.json](https://github.com/z2629321393-ai/changsha-moto-map/blob/fbe5d1a04de07a31f328ebfbe5b6d6e95461a1d2/data/zones.json) | 长沙数据，文件明确写人工近似、需复核；不能移作南京围栏 |
| [The-V-Factor/PeiXiu](https://github.com/The-V-Factor/PeiXiu/tree/1704e8377cd2b0e56ce62cd7bbe387b56c5d5899) | 广州路由研究与样例，未找到南京禁行区文件 |
| [doublenian/motorcycle-map](https://github.com/doublenian/motorcycle-map/tree/cdc68e24d815a26268f8efe2d44d63ca930fd405) | 目录及首页为 Expo 应用脚手架，没有禁行几何数据 |
| [JyzjYzjyZ/motoMap](https://github.com/JyzjYzjyZ/motoMap/tree/2e080aa15bcb0eddc5b933d19d14dcf6550b82c8) | 两份 Lua 文件，主要是道路 ID 集合，没有南京区域多边形、来源或坐标系 |
| [Ultramarine1939-syujie/Moto_nav/js/route.js](https://github.com/Ultramarine1939-syujie/Moto_nav/blob/main/js/route.js) | 实际模式为自行车、驾车及步行，没有南京禁外摩边界库 |

同时检索了 Gitee 索引、南京政务数据开放平台索引及其他地图站点；本次未取得更可靠的
南京矢量下载。检索无结果不证明平台上不存在该数据。

## 接入检查与后续验证

1. 已采用摩途官网区域作为辅助基线，官方附图和实际道路线形保留供交叉核对。
   长江岸线、桥头和辅道的政策归属仍应结合现场标志核对。
2. 已按官网高德渲染方式将数据视为 GCJ-02，保留版本、提取过程及原始文件校验值。
   源数据不再经过 WGS84 转换；手机显示时按现有 MapKit 坐标边界逆变换一次。
3. 建立范围内、范围外、贴边、跨边以及稀疏路线线段的样本，分别核对。
   原南京长江大桥等桥隧规则仍保留单独检查，不由一个城区多边形代替。
4. 完整几何已用于路线检查；上游避让区已经分解并检查覆盖、缝隙和重叠。
   [高德驾车 2.0 文档](https://lbs.amap.com/api/webservice/guide/api/newroute)要求每区至多 16 顶点、
   面积不超过 81 平方公里，默认支持 1 区、最多 32 区。124 点、约 288 平方公里的候选
   **不能作为单区直接传入**；当前为 28 个派生分区，另保留 4 个自定义区域额度，实际 Key 能力待验证。
5. 用真实南京路线检验规划和偏航重算；通过之前继续返回 `partial / review_required`。

公开 Overpass 查询本次返回 HTTP 406、500、502，随后改用 Geofabrik 固定快照，
已经取得上述物理参考线；当前禁行区域源于摩途公开图层，参考线没有被用来猜测替代区域。
派生数据库与后续成果继续保留 [OpenStreetMap 署名与 ODbL 条件](https://www.openstreetmap.org/copyright)。

检索阶段没有修改导航判定，用户确认后已完成源码接入；尚未部署网关，也没有进行真机或实车验证。
