# 摩途电子眼数据核对

核对日期：2026-10-16。用户接受以摩途作为第三方骑行辅助数据源，继续核对南京电子眼。

## 已确认的内容

[摩途官网](https://www.motumap.com/)和
[开发者的 App Store 介绍](https://apps.apple.com/my/app/id1187159082)
均列有电子眼抓拍查询功能。官网加载的
[`electronic_eye_map.js`](https://www.motumap.com/electronic_eye_map.js)
中也确实定义了以下类型：

| 类型值 | 名称 |
| --- | --- |
| 1 | 闯限行 |
| 2 | 应急车道 |
| 3 | 非机动车道 |
| 4 | 闯红灯 |
| 5 | 超速行驶 |
| 6 | 公交车道 |
| 7 | 导向车道 |
| 8 | 车道逆行 |
| 9 | 尾号限行 |
| 10 | 不带头盔 |
| 11 | 噪音超标 |

类型 1 是限行类电子眼，不能把全部电子眼都当成禁摩抓拍。

## 数据入口分开

- [`nodata.js`](https://www.motumap.com/nodata.js)：本次读取 441 条区域／路段记录，
  其中 `IsArea=1` 有 162 条、`IsArea=0` 有 279 条。网页把它们分别绘制为 Polygon／Polyline。
  南京只有 `AreaID=10255` 这一条区域记录，124 个顶点；没有从中取得南京电子眼列表。
- `electronic_eye_map.js`：通过单独接口查询电子眼，不从上述区域文件中读取。
  代码中的位置接口是 `GET https://motu.motumap.com/v1/poi/encodeLocation`；
  电子眼接口是 `POST https://motu.motumap.com/v2/poi/searchRulePoi`，类型筛选字段为 `tabTypes`。
- 代码预期电子眼记录包括 `coordinate.latitude`、`coordinate.longitude`、`tabType` 等字段。
  这是客户端预期格式，尚未用真实南京响应确认，也未确认记录级更新日期。

## 实际请求结果

使用南京地图中心示例坐标 32.05、118.80，没有发送真实车牌。

- 位置接口返回 `code=20106`、`data=null`，消息为“sign校验异常：平台不能为空”。
- 按公开网页代码构造的电子眼 POST 请求没有取得列表；该路径返回 HTTP 404 HTML。
- 公开脚本抓取版本中的 `drawMapPoi()` 和 `handleMarkerClick()` 为空。
  `getElectronicEyeData()` 在请求失败时会回退到 `getMockElectronicEyeData()`，
  后者用随机数生成 10 个 `mock_` 点。**这些点是模拟数据，不能导入真实摄像头库。**

因此，目前能确认摩途产品有电子眼功能、客户端存在限行类别和查询入口，
但本轮没有取得真实南京摄像头坐标，不能报告摄像头数量、覆盖完整性或抓拍方向。
网页入口返回错误也不能推断 App 内没有数据。

脚本快照与错误响应保存于 Git 忽略的 `build/nanjing-boundary-research/`。
电子眼脚本 SHA-256：`f6af7a4ea90bf0ce2d091f3430f66f96b5f30716adc2496d9255e60cce8c190a`。

## 接入处理

禁行区域按用户接受的摩途图层继续作为第三方辅助规则基线，几何与坐标检查属于接入检查。
摄像头层需要通过可用的数据接口或正式导出取得真实点位，再按类型、方向、适用车辆和时间处理。
摄像头数据未取得时应标记“未加载”，不能显示为“没有摄像头”。
区域避让与电子眼提醒各自保留；没有点位不能作为道路可通行的判断依据。

禁行区域随后已按用户确认接入网关和手机，详情见[南京骑行说明](../NANJING_MOTORCYCLE_NAVIGATION.md)。
摄像头尚未添加运行时查询或真实点位功能，手机明确显示“数据未加载”；未部署外部服务。
