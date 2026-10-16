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
              var options = try? JSONDecoder().decode(MotorcycleRouteOptions.self, from: data), options.isValid
        else { return MotorcycleRouteOptions() }
        if let stored = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
           stored["avoid_elevated_roads"] == nil {
            options.avoidElevatedRoads = true
            // Preserve all old preferences; persist the migration so a later
            // explicit switch-off remains off on subsequent launches.
            if let migrated = try? JSONEncoder().encode(options) { defaults.set(migrated, forKey: key) }
        }
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
                    DucatiBrand(caption: "RIDING PREFERENCES")
                }
                Section {
                    Toggle("避开高速", isOn: $draft.avoidHighways)
                        .accessibilityIdentifier("riding-avoid-highways")
                    Toggle("避开高架", isOn: $draft.avoidElevatedRoads)
                        .accessibilityIdentifier("riding-avoid-elevated-roads")
                    Toggle("少收费", isOn: $draft.avoidTolls)
                    Toggle("避开轮渡", isOn: $draft.avoidFerries)
                } header: { Text("路线偏好") } footer: {
                    Text("避开高架默认开启，按高德道路类型检查完整路线及偏航新路线。未找到符合设置的路线时会提示；仍需核对现场通行标志。")
                }
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
                    Text("会剔除穿过这些区域的候选路线；若没有可用路线，请调整区域或添加途经点。左滑可删除。")
                }
                Section {
                    Button { showsWaypoints = true } label: {
                        LabeledContent("在地图上选择", value: "\(draft.waypoints.count)/6")
                    }.accessibilityIdentifier("riding-waypoints")
                    if !draft.waypoints.isEmpty {
                        Button("清空途经点", role: .destructive) { draft.waypoints = [] }
                    }
                } header: { Text("途经点") } footer: { Text("按点击顺序经过所选位置。骑行时只会从后续重算中移除已到达的途经点。") }
                if let errorMessage { Section { Text(errorMessage).foregroundStyle(DucatiTheme.warning) } }
            }
            .disabled(model.isNavigationActive)
            .ducatiList().navigationTitle("骑行设置").navigationBarTitleDisplayMode(.inline)
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
