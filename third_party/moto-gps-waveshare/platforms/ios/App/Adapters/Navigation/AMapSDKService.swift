import Foundation
import JavaScriptCore
import MotoNavigationCore
import Security
import Network

@MainActor
protocol AppNavigationRuntime: AnyObject {
    var onSnapshot: ((MotoNavCoreSnapshot) -> Void)? { get set }
    var onFailure: ((String) -> Void)? { get set }
    var onRouteAssessment: ((MotorcycleRouteAssessment?) -> Void)? { get set }
    var onPolicyStopped: ((String) -> Void)? { get set }
    var activeRoutePolyline: [GCJ02Point] { get }
    func stop()
    func selectDisplayPage(rawValue: UInt8) -> Bool
}
extension SharedNavigationRuntime: AppNavigationRuntime {}

enum AMapSDKSettings {
    static let consentKey = "MotoGPS.AMapSDK.Privacy.v1"
    private static let service = "MotoGPS.AMapSDK"
    static var consent: Bool { UserDefaults.standard.bool(forKey: consentKey) }
    static var key: String {
        var result: CFTypeRef?
        let query: [String: Any] = [kSecClass as String:kSecClassGenericPassword,
            kSecAttrService as String:service, kSecAttrAccount as String:"ios-key",
            kSecReturnData as String:true, kSecMatchLimit as String:kSecMatchLimitOne]
        if SecItemCopyMatching(query as CFDictionary, &result) == errSecSuccess,
           let data = result as? Data, let value = String(data: data, encoding: .utf8) { return value }
        let bundled = Bundle.main.object(forInfoDictionaryKey: "MOTOGPSAMapIOSKey") as? String ?? ""
        return bundled.hasPrefix("$(") ? "" : bundled
    }
    static var ready: Bool { consent && validKey(key) }
    static func validKey(_ key: String) -> Bool {
        key.range(of: "^[a-fA-F0-9]{32}$", options: .regularExpression) != nil
    }
    static func save(key: String, consent: Bool) throws {
        let key = key.trimmingCharacters(in: .whitespacesAndNewlines)
        guard validKey(key), consent else { throw AMapSDKError.configuration }
        let query: [String: Any] = [kSecClass as String:kSecClassGenericPassword,
            kSecAttrService as String:service, kSecAttrAccount as String:"ios-key"]
        let values: [String: Any] = [kSecValueData as String:Data(key.utf8),
            kSecAttrAccessible as String:kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly]
        let status = SecItemUpdate(query as CFDictionary, values as CFDictionary)
        let result = status == errSecItemNotFound
            ? SecItemAdd(query.merging(values) { _, new in new } as CFDictionary, nil) : status
        guard result == errSecSuccess else { throw AMapSDKError.message("无法保存 iOS Key（钥匙串 \(result)）。") }
        UserDefaults.standard.set(true, forKey: consentKey)
    }
}
enum AMapSDKError: LocalizedError {
    case configuration, message(String)
    var errorDescription: String? {
        switch self {
        case .configuration: "请在高德 SDK 设置中填写 iOS Key，并阅读和同意隐私说明。"
        case let .message(message): message
        }
    }
}

@MainActor
final class SDKMotorcyclePolicy {
    private let context: JSContext?
    init() {
        context = JSContext()
        if let url = Bundle.main.url(forResource: "SDKMotorcyclePolicy.generated", withExtension: "js"),
           let script = try? String(contentsOf: url, encoding: .utf8) { context?.evaluateScript(script) }
    }
    func parameters(request: RouteRequest, detour: [GCJ02Point]? = nil, limit: Int = 8) throws -> [String: Any] {
        guard let context, context.exception == nil,
              let function = context.objectForKeyedSubscript("prepareSDKAvoidance"), !function.isUndefined else {
            throw AMapSDKError.message("本地路线偏好检查不可用，请更新 App 后重新规划。")
        }
        let requestJSON = String(decoding: try JSONEncoder().encode(request), as: UTF8.self)
        let lineJSON = try detour.map { String(decoding: try JSONEncoder().encode($0), as: UTF8.self) } ?? ""
        guard let text = function.call(withArguments: [requestJSON, lineJSON, limit])?.toString(),
              let data = text.data(using: .utf8),
              let result = try JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            throw AMapSDKError.message("本地路线偏好检查不可用，请更新 App 后重新规划。")
        }
        guard result["ok"] as? Bool == true, let parameters = result["parameters"] as? [String: Any] else {
            throw AmapGatewayError.rejected(code: result["code"] as? String ?? "ROUTE_PREFERENCE_CONFLICT",
                message: result["message"] as? String ?? "绕行检查未通过", retryable: false)
        }
        return parameters
    }
    func check(request: RouteRequest, route: [String: Any]? = nil, searchSeed: Bool = false) throws -> RoutePlan? {
        guard let context, context.exception == nil,
              let function = context.objectForKeyedSubscript("checkSDKRoute"), !function.isUndefined else {
            throw AMapSDKError.message("本地路线偏好检查不可用，请更新 App 后重新规划。")
        }
        let requestJSON = String(decoding: try JSONEncoder().encode(request), as: UTF8.self)
        let routeJSON = try route.map { String(decoding: try JSONSerialization.data(withJSONObject: $0), as: UTF8.self) } ?? ""
        guard let text = function.call(withArguments: [requestJSON, routeJSON, searchSeed])?.toString(),
              let data = text.data(using: .utf8) else { throw AMapSDKError.message("本地路线偏好检查不可用，请更新 App 后重新规划。") }
        struct Result: Decodable { let ok: Bool; let route: RoutePlan?; let code: String?; let message: String? }
        let result = try JSONDecoder().decode(Result.self, from: data)
        guard result.ok else {
            throw AmapGatewayError.rejected(code: result.code ?? "ROUTE_PREFERENCE_CONFLICT",
                message: result.message ?? "路线检查未通过", retryable: false)
        }
        return result.route
    }
}

/// Owns exactly one SDK manager. All callbacks and policy evaluation stay on the main actor.
@MainActor
final class AMapSDKService {
    let bridge: AMapSDKBridge
    let policy = SDKMotorcyclePolicy()
    private var configuredKey: String?
    private var requestGeneration: UInt64 = 0
    private(set) var previewRequest: RouteRequest?
    init(bridge: AMapSDKBridge = AMapSDKBridge()) { self.bridge = bridge }
    func prepare() throws {
        guard AMapSDKSettings.ready else { throw AMapSDKError.configuration }
        let key = AMapSDKSettings.key
        if configuredKey != key { AMapSDKBridge.configureKey(key); configuredKey = key }
    }
    func search(keywords: String, near origin: WGS84Point?) async throws -> [PlaceSearchResult] {
        try prepare(); try Task.checkCancellation()
        let point = origin.map(ChinaCoordinateTransform.wgs84ToGCJ02)
        let values: [[String: Any]] = try await withCheckedThrowingContinuation { continuation in
            bridge.search(keywords, longitude: point?.longitudeDeg ?? .nan, latitude: point?.latitudeDeg ?? .nan) { values, error in
                if let error { continuation.resume(throwing: error) }
                else { continuation.resume(returning: values ?? []) }
            }
        }
        try Task.checkCancellation()
        return values.compactMap { p in
            guard let lon = p["longitude_deg"] as? Double, let lat = p["latitude_deg"] as? Double,
                  lon.isFinite, lat.isFinite, abs(lon)<=180, abs(lat)<=85 else { return nil }
            let location = ChinaCoordinateTransform.gcj02ToWGS84(GCJ02Point(longitudeDeg: lon, latitudeDeg: lat))
            let city = p["city"] as? String ?? "", district = p["district"] as? String ?? ""
            return PlaceSearchResult(id: p["id"] as? String ?? "", name: p["name"] as? String ?? "",
                address: p["address"] as? String ?? "", city: city, district: district,
                displayArea: city + district, location: location, distanceM: nil)
        }
    }
    func routeOptions(for request: RouteRequest) async throws -> [RoutePlan] {
        try prepare(); try Task.checkCancellation()
        requestGeneration &+= 1
        let generation = requestGeneration
        previewRequest = nil
        bridge.resetRouteCandidates()
        let planner = SDKRoutePlanner(policy: policy)
        let routes = try await planner.routes(for: request) { [self] parameters, avoidanceSearch in
            try Task.checkCancellation()
            guard generation == requestGeneration else { throw CancellationError() }
            let values: [[String: Any]] = try await withCheckedThrowingContinuation { continuation in
                let completion: @Sendable ([[String: Any]]?, Error?) -> Void = { values, error in
                    if let error { continuation.resume(throwing: error) }
                    else { continuation.resume(returning: values ?? []) }
                }
                if avoidanceSearch { bridge.searchAvoiding(parameters, completion: completion) }
                else { bridge.calculate(parameters, completion: completion) }
            }
            try Task.checkCancellation()
            guard generation == requestGeneration else { throw CancellationError() }
            return values
        }
        try Task.checkCancellation()
        guard generation == requestGeneration else { throw CancellationError() }
        previewRequest = request
        return routes
    }
}

/// Search SDK polygons and guidance waypoints support explicit route preferences.
/// Regional motorcycle restrictions are not part of planning or rerouting.
@MainActor
final class SDKRoutePlanner {
    private let policy: SDKMotorcyclePolicy
    init(policy: SDKMotorcyclePolicy) { self.policy = policy }
    func routes(for request: RouteRequest,
                calculate: ([String: Any], Bool) async throws -> [[String: Any]]) async throws -> [RoutePlan] {
        let parameters = try policy.parameters(request: request)
        var lastConflict: Error?
        var elevatedRoadName: String?
        let avoidElevated = request.motorcycleOptions?.avoidElevatedRoads == true
        func checked(_ raw: [[String: Any]], searchSeed: Bool = false) -> [RoutePlan] {
            raw.compactMap { value in
                if avoidElevated, !searchSeed, elevatedRoadName == nil,
                   let links = value["sdk_steps"] as? [[String: Any]] {
                    elevatedRoadName = links.first(where: {
                        ($0["link_type"] as? Int) == 4 &&
                        !($0["road_name"] as? String ?? "").trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
                    })?["road_name"] as? String
                }
                do {
                    guard let route = try policy.check(request: request, route: value, searchSeed: searchSeed),
                          route.totalDistanceM > 0, route.totalDurationS > 0 else { return nil }
                    return route
                } catch {
                    lastConflict = error
                    return nil
                }
            }
        }
        func calculateSafely(_ parameters: [String: Any], search: Bool) async throws -> [[String: Any]]? {
            try Task.checkCancellation()
            do {
                let raw = try await calculate(parameters, search)
                try Task.checkCancellation()
                return raw
            } catch is CancellationError { throw CancellationError() }
            catch {
                try Task.checkCancellation()
                lastConflict = error
                return nil
            }
        }
        let direct = checked(try await calculateSafely(parameters, search: false) ?? [])
        if !direct.isEmpty { return Array(direct.prefix(3)) }
        var nativeParameters = parameters
        if avoidElevated {
            // Strategy 3 proposes an expressway-avoiding alternative; complete
            // link checks still enforce the separately enabled elevated preference.
            var alternateParameters = parameters; alternateParameters["strategy"] = 3
            if let raw = try await calculateSafely(alternateParameters, search: false) {
                nativeParameters = alternateParameters
                let alternate = checked(raw)
                if !alternate.isEmpty { return Array(alternate.prefix(3)) }
            }
        }
        var searchParameters = parameters
        if let elevatedRoadName { searchParameters["avoid_road"] = elevatedRoadName }
        let seeds = checked(try await calculateSafely(searchParameters, search: true) ?? [], searchSeed: true)
        for seed in seeds.prefix(2) {
            var previousHints: Data?
            for limit in [8, 16] {
                var detour = try policy.parameters(request: request, detour: seed.polyline, limit: limit)
                detour["strategy"] = nativeParameters["strategy"]
                let hints = try JSONSerialization.data(withJSONObject: detour["waypoints"] ?? [], options: .sortedKeys)
                if hints == previousHints { continue }
                previousHints = hints
                let routes = checked(try await calculateSafely(detour, search: false) ?? [])
                if !routes.isEmpty { return Array(routes.prefix(3)) }
            }
        }
        throw AmapGatewayError.rejected(code: "ROUTE_PREFERENCE_CONFLICT",
            message: "未找到满足当前路线偏好的路线。请在骑行设置中调整避开高架、高速或自定义避让区域后重试。" +
                (lastConflict.map { "\n" + $0.localizedDescription } ?? ""), retryable: false)
    }
}

@MainActor
final class AMapSDKNavigationRuntime: AppNavigationRuntime {
    var onSnapshot: ((MotoNavCoreSnapshot) -> Void)?
    var onFailure: ((String) -> Void)?
    var onRouteAssessment: ((MotorcycleRouteAssessment?) -> Void)?
    var onPolicyStopped: ((String) -> Void)?
    private(set) var activeRoutePolyline: [GCJ02Point] = []
    private let service: AMapSDKService
    private let request: RouteRequest
    private var active = false
    private var networkMonitor: NWPathMonitor?
    deinit { networkMonitor?.cancel() }
    init(service: AMapSDKService, request: RouteRequest) { self.service = service; self.request = request }
    func start(route: RoutePlan) -> Bool {
        activeRoutePolyline = route.polyline
        active = true
        service.bridge.onSnapshot = { [weak self] snapshot in self?.onSnapshot?(snapshot) }
        service.bridge.onFailure = { [weak self] message in
            self?.active = false
            self?.onPolicyStopped?(message)
        }
        service.bridge.approveRoute = { [weak self] raw in
            guard let self, self.active else { return false }
            do {
                guard let checked = try self.service.policy.check(request: self.request, route: raw) else {
                    throw AMapSDKError.message("本地路线偏好检查不可用，请更新 App 后重新规划。")
                }
                self.activeRoutePolyline = checked.polyline
                self.onRouteAssessment?(checked.motorcycleAssessment)
                return true
            } catch {
                self.active = false
                // Native bridge cancels immediately; deliver the UI change after that cancelled snapshot.
                DispatchQueue.main.async { [weak self] in
                    self?.onPolicyStopped?("新路线不满足路线偏好，已停止导航。请重新规划。\n" + error.localizedDescription)
                }
                return false
            }
        }
        let monitor = NWPathMonitor()
        networkMonitor = monitor
        monitor.pathUpdateHandler = { [weak self] path in
            let available = path.status == .satisfied
            Task { @MainActor [weak self] in
                guard self?.active == true else { return }
                self?.service.bridge.setNetworkAvailable(available)
            }
        }
        monitor.start(queue: DispatchQueue(label: "MotoGPS.SDK.Network"))
        let started = service.bridge.startRoute(route.routeID)
        if !started { active = false; onFailure?("所选 SDK 路线已失效，请重新规划。") }
        return started
    }
    func stop() {
        active = false
        networkMonitor?.cancel(); networkMonitor = nil
        service.bridge.stop()
        service.bridge.onSnapshot = nil; service.bridge.onFailure = nil; service.bridge.approveRoute = nil
        activeRoutePolyline = []
    }
    func selectDisplayPage(rawValue: UInt8) -> Bool { service.bridge.selectPage(rawValue) }
}
