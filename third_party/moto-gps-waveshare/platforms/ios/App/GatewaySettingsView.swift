import SwiftUI

struct GatewaySettingsView: View {
    @ObservedObject var model: AppModel
    @Environment(\.dismiss) private var dismiss
    @State private var key = ""
    @State private var consent = false
    @State private var message: String?
    @State private var showsMapGateway = false
    var body: some View {
        NavigationStack {
            Form {
                Section("高德 iOS SDK") {
                    SecureField("iOS 平台 Key", text: $key)
                        .textInputAutocapitalization(.never).autocorrectionDisabled()
                        .accessibilityIdentifier("amap-ios-key")
                    Text("Bundle ID：\(Bundle.main.bundleIdentifier ?? "")")
                        .font(.footnote).textSelection(.enabled)
                    Text("请在高德控制台把这个 Bundle ID 绑定到 iOS 平台 Key。Web 服务 Key 不能用于此处。Key 保存在本机钥匙串。")
                        .font(.footnote).foregroundStyle(.secondary)
                }
                Section("隐私与授权") {
                    Text("启用后，高德软件有限公司提供的地图、搜索与导航 SDK 将直接处理搜索词、起终点、途经点、导航位置，以及其隐私政策列明的设备与网络信息。绕行规划会向高德提交限摩图层和你设置的避让区域坐标；车辆登记类型在本机用于选择规则。")
                    Link("阅读高德 SDK 隐私政策", destination: URL(string: "https://lbs.amap.com/pages/privacy/")!)
                    Link("阅读高德开放平台服务协议", destination: URL(string: "https://lbs.amap.com/home/terms/")!)
                    Toggle("已阅读并同意使用高德 SDK", isOn: $consent)
                        .accessibilityIdentifier("amap-sdk-consent")
                    Text("保存后才初始化 SDK。当前使用驾车算路和本地南京规则筛选，专用摩托车算路需另行开通。")
                        .font(.footnote).foregroundStyle(.secondary)
                }
                Section("车机周边底图") {
                    Text("搜索、算路和导航不需要自建网关。圆屏显示路线与转向；新增道路、建筑底图及离线包下载仍需可选底图网关，已有缓存继续可用。")
                    Button("配置可选底图网关") { showsMapGateway = true }
                }
                if let message { Section { Text(message).foregroundStyle(.red) } }
            }
            .scrollContentBackground(.hidden).ducatiScreen()
            .navigationTitle("高德 SDK 设置").navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) { Button("取消") { dismiss() } }
                ToolbarItem(placement: .confirmationAction) {
                    Button("保存") {
                        do { try model.saveSDKConfiguration(key: key, consent: consent); dismiss() }
                        catch { message = error.localizedDescription }
                    }.disabled(model.isNavigationActive || !consent || key.isEmpty)
                }
            }
            .onAppear { key = AMapSDKSettings.key; consent = AMapSDKSettings.consent }
            .sheet(isPresented: $showsMapGateway) { MapGatewaySettingsView(model: model) }
        }.ducatiAppTheme()
    }
}
