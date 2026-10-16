import SwiftUI

// Ducati V2 personal UI adaptation, 2026-10-16: shared dark list styling.
/// Available without a connection, so permission and storage information can
/// still be read when the gateway or the accessory is unavailable.
struct DataUseView: View {
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            List {
                Section("位置与路线") {
                    Text("地点搜索、规划和重新规划路线时，搜索内容、起终点及所需位置会发送给配置的导航服务。南京骑行设置中的车辆登记类别、途经点与自定义避让区域也会发送到网关，用于路线选择和检查；算路所需的途经点、避让区域会发送给高德。在线周边地图请求也会透露所查看的区域。")
                    Text("导航期间，定位用于更新路线和圆屏指引；锁屏或切换应用后仍可继续。结束导航会停止导航定位。可在系统设置中调整权限。")
                    Text("南京禁外摩区域采用摩途官网公开图层快照，由网关用于避让和路线检查。当前不会向摩途发送你的车牌或实时位置，限行电子眼数据尚未接入。")
                }
                Section("蓝牙与音乐") {
                    Text("连接圆屏后，导航、速度及周边地图通过蓝牙同步。允许媒体资料库权限后，Apple Music 的曲名、歌手和播放状态也会同步到圆屏，圆屏可控制上一首、播放或暂停、下一首。")
                    Text("音乐信息不发送到 MOTO GPS 的导航服务器。拒绝音乐权限不影响地点搜索和导航。")
                }
                Section("存储与清理") {
                    Text("最近地点、骑行设置、自定义避让区域、途经点、已连接设备标识和下载地图保存在手机上。首页可清空最近搜索，南京骑行设置可删除区域与途经点；地图与离线下载页面可删除下载包、清理临时缓存。删除 App 可移除它的本地数据；系统备份可能包含部分本地数据。")
                    Text("服务器访问日志可能保留 IP、请求时间、搜索词、位置参数及地图区域。记录范围、轮转和保留期限由部署方的实际配置及隐私政策说明；这不等于数据只在手机上保存。")
                }
                Section {
                    Link("高德隐私政策", destination: URL(string: "https://lbs.amap.com/home/privacy/")!)
                    Link("Apple 隐私政策", destination: URL(string: "https://www.apple.com/legal/privacy/")!)
                    Link("OpenStreetMap 数据与许可", destination: URL(string: "https://www.openstreetmap.org/copyright")!)
                    Link("Protomaps 地图数据", destination: URL(string: "https://protomaps.com")!)
                    Link("摩途禁行区域数据", destination: URL(string: "https://www.motumap.com/")!)
                } header: {
                    Text("第三方服务")
                } footer: {
                    Text("手机路线底图由 Apple 地图提供，圆屏周边道路与建筑使用 OpenStreetMap / Protomaps 数据。")
                }
            }
            .ducatiList()
            .navigationTitle("隐私与数据")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("完成") { dismiss() }
                        .accessibilityIdentifier("privacy-data-done")
                }
            }
        }.ducatiAppTheme()
    }
}
