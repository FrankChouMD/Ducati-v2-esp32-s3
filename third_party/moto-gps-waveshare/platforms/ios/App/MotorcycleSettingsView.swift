import Foundation
import MapKit
import MotoNavigationCore
import SwiftUI

// Ducati V2 personal adaptation, 2026-10-16.
enum MotorcyclePreferenceError: LocalizedError {
    case invalid, navigationActive
    var errorDescription: String? {
        switch self {
        case .invalid: "骑行设置或避让区域无效，请检查后重试。"
        case .navigationActive: "请先结束导航，再修改骑行设置。"
        }
    }
}

enum MotorcyclePreferenceStore {
    static let key = "DucatiV2.MotorcycleOptions.v1"
    static func load(defaults: UserDefaults = .standard) -> MotorcycleRouteOptions {
        guard let data = defaults.data(forKey: key),
              let options = try? JSONDecoder().decode(MotorcycleRouteOptions.self, from: data), options.isValid
        else { return MotorcycleRouteOptions() } // User selected Nanjing suburban registration.
        return options
    }
    static func save(_ options: MotorcycleRouteOptions, defaults: UserDefaults = .standard) throws {
        guard options.isValid else { throw MotorcyclePreferenceError.invalid }
        defaults.set(try JSONEncoder().encode(options), forKey: key)
    }
}

struct MotorcycleSettingsView: View {
    @ObservedObject var model: AppModel
    @Environment(\.dismiss) private var dismiss
    @State private var draft: MotorcycleRouteOptions
    @State private var editingArea: MotorcycleAvoidArea?
    @State private var showsWaypoints = false
    @State private var errorMessage: String?

    init(model: AppModel) {
        self.model = model
        _draft = State(initialValue: model.ridingOptions)
    }

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    DucatiBrand(caption: "NANJING RIDING")
                    Picker("车辆登记类型", selection: $draft.registration) {
                        ForEach(MotorcycleRegistration.allCases, id: \.self) { registration in
                            Text(registration.title).tag(registration)
                        }
                    }.accessibilityIdentifier("riding-registration")
                } footer: {
                    Text("当前默认：南京郊区登记号牌。江宁、浦口、六合等登记类别不能仅凭苏A判断主城区通行资格。")
                }
                Section {
                    Toggle("避开高速", isOn: $draft.avoidHighways)
                        .accessibilityIdentifier("riding-avoid-highways")
                    Toggle("少收费", isOn: $draft.avoidTolls)
                    Toggle("避开轮渡", isOn: $draft.avoidFerries)
                } header: { Text("路线偏好") } footer: { Text("使用普通驾车接口算路，偏好不能代替摩托车禁限行规则。") }
                Section {
                    ForEach(draft.avoidAreas) { area in
                        Button { editingArea = area } label: {
                            LabeledContent(area.name, value: "\(area.points.count) 个顶点")
                        }
                    }.onDelete { draft.avoidAreas.remove(atOffsets: $0) }
                    Button {
                        editingArea = MotorcycleAvoidArea(name: "避让区域\(draft.avoidAreas.count + 1)", points: [])
                    } label: { Label("在地图上添加区域", systemImage: "plus") }
                    .disabled(draft.avoidAreas.count >= 4)
                    .accessibilityIdentifier("riding-add-avoid-area")
                } header: { Text("自定义避让区域 · \(draft.avoidAreas.count)/4") } footer: {
                    Text("自定义区域会用于算路，并检查完整路线是否穿过边界。它们是你设置的避让范围，不是官方禁行边界。左滑可删除。")
                }
                Section {
                    Button { showsWaypoints = true } label: {
                        LabeledContent("在地图上选择", value: "\(draft.waypoints.count)/6")
                    }.accessibilityIdentifier("riding-waypoints")
                    if !draft.waypoints.isEmpty {
                        Button("清空途经点", role: .destructive) { draft.waypoints = [] }
                    }
                } header: { Text("途经点") } footer: { Text("按点击顺序经过所选位置。骑行时只会从后续重算中移除已到达的途经点。") }
                Section {
                    Text(draft.registration == .nanjingUrban
                         ? "主城区登记号牌不套用禁外摩区域避让，已收录桥隧及自定义避让区仍会检查。"
                         : "自动避让摩途官网南京禁外摩区域，并复查完整路线是否穿区。路线地图以橙色显示该区域。")
                    Text("同时检查已收录桥隧与自定义避让区。城市高架、分时和临时管制未完整收录。")
                    Label("限行电子眼 · 数据未加载", systemImage: "video.slash")
                    Link("摩途官网禁行图层", destination: URL(string: "https://www.motumap.com/")!)
                    Link("南京市政府禁外摩通告与附图", destination: URL(string: "https://www.nanjing.gov.cn/xxgkn/szgfxwj/202311/t20231115_4098444.html")!)
                        .foregroundStyle(DucatiTheme.warning)
                    Link("禁外摩范围与适用车辆", destination: URL(string: "https://www.nanjing.gov.cn/zdgk/200805/t20080528_1054634.html")!)
                    Link("南京市交通运输局过江通道答复", destination: URL(string: "https://www.nanjing.gov.cn/xxgkn/jytabljggk/2025njytabl/shirddbjy/202512/t20251205_5706046.html")!)
                    Link("新生圩长江大桥通行政策", destination: URL(string: "https://jtj.nanjing.gov.cn/njsjtysj/202511/t20251121_5695260.html")!)
                    Link("建宁路长江隧道通行政策", destination: URL(string: "https://www.nanjing.gov.cn/hdjl/hygq/202608/t20260819_5896120.html")!)
                } header: { Text("南京规则覆盖范围") } footer: { Text("资料核对日期：2026-10-16。请同时核对最新公告与现场标志。") }
                if let errorMessage { Section { Text(errorMessage).foregroundStyle(DucatiTheme.warning) } }
            }
            .disabled(model.isNavigationActive)
            .ducatiList().navigationTitle("南京骑行").navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) { Button("取消") { dismiss() } }
                ToolbarItem(placement: .confirmationAction) {
                    Button("保存") {
                        do { try model.saveRidingOptions(draft); dismiss() }
                        catch { errorMessage = error.localizedDescription }
                    }.disabled(model.isNavigationActive || !draft.isValid)
                    .accessibilityIdentifier("riding-save")
                }
            }
            .sheet(item: $editingArea) { area in
                MotorcycleMapEditor(isArea: true, title: area.name, points: area.points) { name, points in
                    let updated = MotorcycleAvoidArea(id: area.id, name: name, points: points)
                    if let index = draft.avoidAreas.firstIndex(where: { $0.id == area.id }) { draft.avoidAreas[index] = updated }
                    else { draft.avoidAreas.append(updated) }
                }
            }
            .sheet(isPresented: $showsWaypoints) {
                MotorcycleMapEditor(isArea: false, title: "途经点", points: draft.waypoints) { _, points in
                    draft.waypoints = points
                }
            }
        }.ducatiAppTheme()
    }
}

struct MotorcycleAssessmentCard: View {
    let assessment: MotorcycleRouteAssessment
    var review: Binding<Bool>? = nil

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Label("南京骑行 · 规则覆盖不完整", systemImage: "exclamationmark.triangle")
                .font(.subheadline.weight(.semibold)).foregroundStyle(DucatiTheme.warning)
            Text(assessment.warnings.first ?? "请核对摩托车通行规定。")
                .font(.caption).fixedSize(horizontal: false, vertical: true)
            if let area = assessment.restrictionAreas?.first, area.isValid {
                Label(area.appliesToRegistration ? "橙色区域 · 摩途禁外摩区，已用于避让" : "禁外摩区仅供参考，当前登记类型未套用",
                      systemImage: "map")
                    .font(.caption).foregroundStyle(DucatiTheme.warning)
            }
            Label("限行电子眼 · 数据未加载", systemImage: "video.slash")
                .font(.caption).foregroundStyle(DucatiTheme.secondary)
            DisclosureGroup("覆盖范围与规则来源") {
                VStack(alignment: .leading, spacing: 12) {
                    ForEach(Array(assessment.warnings.dropFirst().enumerated()), id: \.offset) { _, warning in
                        Text(warning).font(.caption).foregroundStyle(DucatiTheme.secondary)
                    }
                    ForEach(Array(assessment.sources.enumerated()), id: \.offset) { _, source in
                        if let url = URL(string: source.url), url.scheme == "https" {
                            Link(source.title, destination: url).font(.caption).frame(minHeight: 44, alignment: .leading)
                        }
                    }
                    Text("资料核对：\(assessment.verifiedOn)").font(.caption2).foregroundStyle(DucatiTheme.secondary)
                }.padding(.top, 12)
            }.font(.caption)
            if let review {
                Toggle("我已核对本次路线与禁行范围", isOn: review)
                    .font(.subheadline).accessibilityIdentifier("riding-route-reviewed")
            }
        }.ducatiCard().accessibilityIdentifier("riding-rule-assessment")
    }
}

private struct MotorcycleMapEditor: View {
    let isArea: Bool
    let onSave: (String, [WGS84Point]) -> Void
    @Environment(\.dismiss) private var dismiss
    @State private var name: String
    @State private var points: [WGS84Point]
    @State private var position: MapCameraPosition

    init(isArea: Bool, title: String, points: [WGS84Point], onSave: @escaping (String, [WGS84Point]) -> Void) {
        self.isArea = isArea
        self.onSave = onSave
        _name = State(initialValue: title)
        _points = State(initialValue: points)
        let center = points.first ?? WGS84Point(longitudeDeg: 118.7969, latitudeDeg: 32.0603)
        _position = State(initialValue: .region(MKCoordinateRegion(
            center: CLLocationCoordinate2D(latitude: center.latitudeDeg, longitude: center.longitudeDeg),
            span: MKCoordinateSpan(latitudeDelta: 0.12, longitudeDelta: 0.14))))
    }

    private var validationMessage: String? {
        isArea ? MotorcycleAvoidArea(name: name, points: points).validationMessage : nil
    }

    var body: some View {
        NavigationStack {
            VStack(spacing: 16) {
                if isArea {
                    TextField("区域名称", text: $name).padding(.horizontal, 20)
                        .onChange(of: name) { _, value in name = String(value.prefix(32)) }
                }
                MapReader { proxy in
                    Map(position: $position) {
                        if isArea && points.count >= 3 {
                            MapPolygon(coordinates: coordinates)
                                .foregroundStyle(DucatiTheme.accent.opacity(0.2))
                                .stroke(DucatiTheme.accent, lineWidth: 2)
                        } else if points.count >= 2 {
                            MapPolyline(coordinates: coordinates).stroke(DucatiTheme.accent, lineWidth: 2)
                        }
                        ForEach(Array(points.enumerated()), id: \.offset) { index, point in
                            Marker("\(index + 1)", coordinate: CLLocationCoordinate2D(latitude: point.latitudeDeg, longitude: point.longitudeDeg))
                        }
                    }
                    .mapStyle(.standard(elevation: .flat, pointsOfInterest: .excludingAll))
                    .onTapGesture { location in
                        guard points.count < (isArea ? 16 : 6), let c = proxy.convert(location, from: .local) else { return }
                        points.append(WGS84Point(longitudeDeg: c.longitude, latitudeDeg: c.latitude))
                    }
                }
                .frame(minHeight: 260).accessibilityIdentifier("riding-map-editor")
                VStack(alignment: .leading, spacing: 12) {
                    Text(isArea ? "依次点击地图，圈出需要避让的区域。" : "按经过顺序点击地图，添加途经点。")
                        .font(.subheadline)
                    HStack {
                        Text("已选 \(points.count)/\(isArea ? 16 : 6)").foregroundStyle(DucatiTheme.secondary)
                        Spacer()
                        Button("撤销") { if !points.isEmpty { points.removeLast() } }.disabled(points.isEmpty)
                        Button("重选") { points = [] }.disabled(points.isEmpty)
                    }.frame(minHeight: 44)
                    if let validationMessage { Text(validationMessage).font(.caption).foregroundStyle(DucatiTheme.warning) }
                }.padding(.horizontal, 20).padding(.bottom, 16)
            }
            .ducatiScreen().navigationTitle(isArea ? "编辑避让区域" : "编辑途经点")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) { Button("取消") { dismiss() } }
                ToolbarItem(placement: .confirmationAction) {
                    Button("保存") { onSave(name, points); dismiss() }
                        .disabled(isArea && validationMessage != nil)
                }
            }
        }.ducatiAppTheme()
    }

    private var coordinates: [CLLocationCoordinate2D] {
        points.map { CLLocationCoordinate2D(latitude: $0.latitudeDeg, longitude: $0.longitudeDeg) }
    }
}
