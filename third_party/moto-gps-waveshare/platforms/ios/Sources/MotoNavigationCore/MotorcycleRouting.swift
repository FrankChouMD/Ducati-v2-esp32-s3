import Foundation

// Ducati V2 personal adaptation, 2026-10-16. Coordinates in preferences are
// WGS84; the gateway alone converts them to the route provider's GCJ-02.
public enum MotorcycleRegistration: String, Codable, CaseIterable, Hashable, Sendable {
    case nanjingSuburban = "nanjing_suburban"
    case nanjingUrban = "nanjing_urban"
    case nonNanjing = "non_nanjing"
    case unknown

    public var title: String {
        switch self {
        case .nanjingSuburban: "南京郊区登记号牌"
        case .nanjingUrban: "南京主城区登记号牌"
        case .nonNanjing: "外地号牌"
        case .unknown: "登记类型未确认"
        }
    }
}

public struct MotorcycleAvoidArea: Codable, Equatable, Sendable, Identifiable {
    public var id: String
    public var name: String
    public var points: [WGS84Point]

    public init(id: String = UUID().uuidString, name: String, points: [WGS84Point]) {
        self.id = id
        self.name = name
        self.points = points
    }

    public var validationMessage: String? {
        guard !name.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty, name.count <= 32,
              (3 ... 16).contains(points.count), points.allSatisfy({ $0.isValid && abs($0.latitudeDeg) <= 85 }),
              Set(points).count == points.count else { return "请填写名称，并选择 3–16 个不同的地图顶点。" }
        let n = points.count
        for i in 0 ..< n {
            for j in (i + 1) ..< n {
                if j == i + 1 || (i == 0 && j == n - 1) { continue }
                if Self.intersects(points[i], points[(i + 1) % n], points[j], points[(j + 1) % n]) {
                    return "区域边界不能交叉，请撤销顶点后重选。"
                }
            }
        }
        let origin = points[0]
        let metresLon = 111_320 * cos(origin.latitudeDeg * .pi / 180)
        let local = points.map { (($0.longitudeDeg - origin.longitudeDeg) * metresLon,
                                  ($0.latitudeDeg - origin.latitudeDeg) * 111_320) }
        let area = abs(local.indices.reduce(0.0) { sum, i in
            let p = local[i], q = local[(i + 1) % n]
            return sum + p.0 * q.1 - q.0 * p.1
        }) / 2
        return (4 ... 80_000_000).contains(area) ? nil : "单个区域需在 4 平方米至 80 平方公里之间。"
    }

    private static func intersects(_ a: WGS84Point, _ b: WGS84Point, _ c: WGS84Point, _ d: WGS84Point) -> Bool {
        func cross(_ a: WGS84Point, _ b: WGS84Point, _ p: WGS84Point) -> Double {
            (b.longitudeDeg - a.longitudeDeg) * (p.latitudeDeg - a.latitudeDeg) -
                (b.latitudeDeg - a.latitudeDeg) * (p.longitudeDeg - a.longitudeDeg)
        }
        func on(_ a: WGS84Point, _ b: WGS84Point, _ p: WGS84Point) -> Bool {
            abs(cross(a, b, p)) <= 1e-12 &&
                p.longitudeDeg >= min(a.longitudeDeg, b.longitudeDeg) - 1e-12 &&
                p.longitudeDeg <= max(a.longitudeDeg, b.longitudeDeg) + 1e-12 &&
                p.latitudeDeg >= min(a.latitudeDeg, b.latitudeDeg) - 1e-12 &&
                p.latitudeDeg <= max(a.latitudeDeg, b.latitudeDeg) + 1e-12
        }
        let ac = cross(a, b, c), ad = cross(a, b, d), ca = cross(c, d, a), cb = cross(c, d, b)
        let oppositeAB = (ac > 1e-12 && ad < -1e-12) || (ac < -1e-12 && ad > 1e-12)
        let oppositeCD = (ca > 1e-12 && cb < -1e-12) || (ca < -1e-12 && cb > 1e-12)
        return (oppositeAB && oppositeCD) || on(a, b, c) || on(a, b, d) || on(c, d, a) || on(c, d, b)
    }
}

public struct MotorcycleRouteOptions: Codable, Equatable, Sendable {
    public var region: String
    public var registration: MotorcycleRegistration
    public var avoidHighways: Bool
    public var avoidElevatedRoads: Bool
    public var avoidTolls: Bool
    public var avoidFerries: Bool
    public var avoidAreas: [MotorcycleAvoidArea]
    public var waypoints: [WGS84Point]

    public init(registration: MotorcycleRegistration = .nanjingSuburban,
                avoidHighways: Bool = true, avoidElevatedRoads: Bool = true,
                avoidTolls: Bool = false, avoidFerries: Bool = false,
                avoidAreas: [MotorcycleAvoidArea] = [], waypoints: [WGS84Point] = []) {
        region = "nanjing"
        self.registration = registration
        self.avoidHighways = avoidHighways
        self.avoidElevatedRoads = avoidElevatedRoads
        self.avoidTolls = avoidTolls
        self.avoidFerries = avoidFerries
        self.avoidAreas = avoidAreas
        self.waypoints = waypoints
    }

    public init(from decoder: Decoder) throws {
        let values = try decoder.container(keyedBy: CodingKeys.self)
        region = try values.decode(String.self, forKey: .region)
        registration = try values.decode(MotorcycleRegistration.self, forKey: .registration)
        avoidHighways = try values.decode(Bool.self, forKey: .avoidHighways)
        // A legacy response did not check elevated roads. Preference storage
        // migrates separately so an old assessment cannot claim the new check.
        avoidElevatedRoads = try values.decodeIfPresent(Bool.self, forKey: .avoidElevatedRoads) ?? false
        avoidTolls = try values.decode(Bool.self, forKey: .avoidTolls)
        avoidFerries = try values.decode(Bool.self, forKey: .avoidFerries)
        avoidAreas = try values.decode([MotorcycleAvoidArea].self, forKey: .avoidAreas)
        waypoints = try values.decode([WGS84Point].self, forKey: .waypoints)
    }

    public var isValid: Bool {
        region == "nanjing" && avoidAreas.count <= 4 && waypoints.count <= 6 &&
            avoidAreas.allSatisfy { $0.validationMessage == nil &&
                $0.id.range(of: "^[A-Za-z0-9_-]{1,64}$", options: .regularExpression) != nil } &&
            Set(avoidAreas.map(\.id)).count == avoidAreas.count &&
            waypoints.allSatisfy { $0.isValid && abs($0.latitudeDeg) <= 85 }
    }

    /// Consume only the next ordered waypoint, using a recent accurate fix.
    /// Rerouting must not send riders back to a waypoint already reached.
    public func advancingWaypoints(with fix: NavigationFix, now: Date = Date()) -> Self {
        guard let next = waypoints.first, fix.coordinate.isValid,
              fix.horizontalAccuracyM.isFinite, (0 ... 20).contains(fix.horizontalAccuracyM),
              abs(now.timeIntervalSince(fix.timestamp)) <= 10 else { return self }
        let latitude = (next.latitudeDeg + fix.coordinate.latitudeDeg) * .pi / 360
        let dx = (next.longitudeDeg - fix.coordinate.longitudeDeg) * .pi / 180 * 6_371_000 * cos(latitude)
        let dy = (next.latitudeDeg - fix.coordinate.latitudeDeg) * .pi / 180 * 6_371_000
        guard hypot(dx, dy) <= 30 else { return self }
        var remaining = self
        remaining.waypoints.removeFirst()
        return remaining
    }

    enum CodingKeys: String, CodingKey {
        case region, registration, waypoints
        case avoidHighways = "avoid_highways"
        case avoidElevatedRoads = "avoid_elevated_roads"
        case avoidTolls = "avoid_tolls"
        case avoidFerries = "avoid_ferries"
        case avoidAreas = "avoid_areas"
    }
}

public struct MotorcycleRuleSource: Codable, Equatable, Sendable {
    public let title: String
    public let url: String
}

public struct MotorcycleRestrictionArea: Codable, Equatable, Sendable, Identifiable {
    public let id: String
    public let name: String
    public let sourceURL: String
    public let dataVersion: String
    public let coordinateSystem: String
    public let appliesToRegistration: Bool
    public let polygon: [GCJ02Point]

    public var isValid: Bool {
        id == "motu-025-10255" && !name.isEmpty &&
            sourceURL == "https://www.motumap.com/nodata.js" &&
            dataVersion.hasPrefix("motu-025-10255-") && coordinateSystem == "GCJ-02" &&
            (3 ... 512).contains(polygon.count) && Set(polygon).count == polygon.count &&
            polygon.allSatisfy { $0.longitudeDeg.isFinite && $0.latitudeDeg.isFinite &&
                (-180 ... 180).contains($0.longitudeDeg) && (-85 ... 85).contains($0.latitudeDeg) }
    }

    enum CodingKeys: String, CodingKey {
        case id, name, polygon
        case sourceURL = "source_url"
        case dataVersion = "data_version"
        case coordinateSystem = "coordinate_system"
        case appliesToRegistration = "applies_to_registration"
    }
}

public struct MotorcycleRouteAssessment: Codable, Equatable, Sendable {
    public let policyVersion: String
    public let verifiedOn: String
    public let coverage: String
    public let status: String
    public let checkedAtMs: UInt64
    public let appliedOptions: MotorcycleRouteOptions
    public let warnings: [String]
    public let sources: [MotorcycleRuleSource]
    // Optional decoding makes old gateways fail the policy match explicitly,
    // instead of silently accepting an assessment without area checks.
    public let restrictionAreas: [MotorcycleRestrictionArea]?
    public let cameraDataStatus: String?
    public let restrictionConflicts: [String]?
    public var hasRestrictionConflicts: Bool { !(restrictionConflicts?.isEmpty ?? true) }

    public func matches(_ options: MotorcycleRouteOptions) -> Bool {
        guard appliedOptions == options && coverage == "partial" && status == "review_required" &&
                !policyVersion.isEmpty && !warnings.isEmpty && !sources.isEmpty,
              let restrictionAreas, restrictionAreas.count == 1,
              let area = restrictionAreas.first, area.isValid else { return false }
        return area.appliesToRegistration == (options.registration != .nanjingUrban)
    }

    enum CodingKeys: String, CodingKey {
        case coverage, status, warnings, sources
        case policyVersion = "policy_version"
        case verifiedOn = "verified_on"
        case checkedAtMs = "checked_at_ms"
        case appliedOptions = "applied_options"
        case restrictionAreas = "restriction_areas"
        case cameraDataStatus = "camera_data_status"
        case restrictionConflicts = "restriction_conflicts"
    }
}
