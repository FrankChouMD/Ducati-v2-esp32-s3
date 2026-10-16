import Foundation
import MotoNavigationCore
import XCTest
@testable import MOTO_GPS

@MainActor
final class AMapSDKPolicyTests: XCTestCase {
    private func request(avoidElevated: Bool = true) -> RouteRequest {
        RouteRequest(requestID: 1, origin: point(118.7, 31.9), destination: point(118.9, 31.9),
                     motorcycleOptions: MotorcycleRouteOptions(avoidElevatedRoads: avoidElevated))
    }
    private func point(_ longitude: Double, _ latitude: Double) -> WGS84Point {
        WGS84Point(longitudeDeg: longitude, latitudeDeg: latitude)
    }
    private func rawRoute(_ points: [WGS84Point], id: String = "native-sdk-test") -> [String: Any] {
        let coordinates = points.map(ChinaCoordinateTransform.wgs84ToGCJ02)
        return ["schema_version":1,"route_id":id,"provider":"amap_ios_sdk",
            "coordinate_system":"GCJ-02","generated_at_ms":1,"total_distance_m":18000,"total_duration_s":1500,
            "polyline":coordinates.map { ["longitude_deg":$0.longitudeDeg,"latitude_deg":$0.latitudeDeg] },
            "maneuvers":[],"traffic":[],"sdk_steps":[],"sdk_has_highway":false,"sdk_has_ferry":false,
            "sdk_has_elevated":false,"sdk_elevated_checked":true]
    }
    private var crossing: [String: Any] { rawRoute([request().origin, point(118.784, 32.041), request().destination], id: "crossing") }
    private var outside: [String: Any] { rawRoute([request().origin, request().destination], id: "accepted") }
    private var seed: [String: Any] {
        let points: [WGS84Point] = (0 ... 80).map { i in
            let longitude = 118.7 + Double(i)*0.0025
            let latitude = 31.85 + abs(Double(i)-40)*0.00125
            return point(longitude, latitude)
        }
        var route = rawRoute(points, id: "search-seed")
        route["provider"] = "amap_ios_search_sdk"
        route.removeValue(forKey: "sdk_has_elevated")
        route.removeValue(forKey: "sdk_elevated_checked")
        return route
    }
    func testBundledJavaScriptCorePolicyProducesDecodableRoute() throws {
        let r = request(), policy = SDKMotorcyclePolicy()
        let route = try XCTUnwrap(policy.check(request: r, route: outside))
        XCTAssertEqual(route.provider, "amap_ios_sdk")
        XCTAssertNil(route.motorcycleAssessment)
        let parameters = try policy.parameters(request: r)
        XCTAssertEqual((parameters["avoid_polygons"] as? [Any])?.count, 0)
        XCTAssertEqual(parameters["search_strategy"] as? Int, 40)
        XCTAssertEqual(parameters["strategy"] as? Int, 15)
    }
    func testPolicyDoesNotAcceptRequestWithoutRiderSettings() {
        let r = request()
        XCTAssertThrowsError(try SDKMotorcyclePolicy().check(request: RouteRequest(requestID: 2, origin: r.origin, destination: r.destination)))
    }
    func testRestrictedEndpointsPlanDirectlyWithoutAssessmentForEveryRegistration() async throws {
        for registration in MotorcycleRegistration.allCases {
            let inside = point(118.784, 32.041)
            let r = RouteRequest(requestID: 2, origin: inside, destination: point(118.79,32.05),
                motorcycleOptions: MotorcycleRouteOptions(registration: registration, waypoints: [inside]))
            var calls = 0
            let routes = try await SDKRoutePlanner(policy: SDKMotorcyclePolicy()).routes(for: r) { parameters, search in
                calls += 1; XCTAssertFalse(search)
                XCTAssertEqual((parameters["avoid_polygons"] as? [Any])?.count, 0)
                XCTAssertEqual((parameters["waypoints"] as? [Any])?.count, 1)
                return [self.rawRoute([r.origin, r.destination])]
            }
            XCTAssertEqual(calls, 1)
            XCTAssertEqual(routes.count, 1)
            XCTAssertNil(routes.first?.motorcycleAssessment)
        }
    }
    func testCrossingAndOutsideCandidatesAreBothAvailableImmediately() async throws {
        var calls = 0
        let routes = try await SDKRoutePlanner(policy: SDKMotorcyclePolicy()).routes(for: request()) { _, search in
            calls += 1; XCTAssertFalse(search); return [self.crossing, self.outside]
        }
        XCTAssertEqual(calls, 1); XCTAssertEqual(routes.map(\.routeID), ["crossing", "accepted"])
    }
    func testCustomAreaCrossingStillTriggersSearchAndCheckedNativeDetour() async throws {
        let manual = MotorcycleAvoidArea(id: "saved", name: "自定义区域",
            points: [point(118.78,32.035),point(118.79,32.035),point(118.79,32.045),point(118.78,32.045)])
        let r = RouteRequest(requestID: 10, origin: request().origin, destination: request().destination,
                             motorcycleOptions: MotorcycleRouteOptions(avoidAreas: [manual]))
        var calls = 0
        let routes = try await SDKRoutePlanner(policy: SDKMotorcyclePolicy()).routes(for: r) { parameters, search in
            calls += 1
            XCTAssertEqual((parameters["avoid_polygons"] as? [Any])?.count, 1)
            if calls == 1 { XCTAssertFalse(search); return [self.crossing] }
            if calls == 2 { XCTAssertFalse(search); XCTAssertEqual(parameters["strategy"] as? Int, 3); return [self.crossing] }
            if calls == 3 { XCTAssertTrue(search); return [self.seed] }
            XCTAssertFalse(search)
            XCTAssertEqual((parameters["waypoints"] as? [Any])?.count, calls == 4 ? 8 : 16)
            return calls == 4 ? [self.crossing] : [self.outside]
        }
        XCTAssertEqual(calls, 5); XCTAssertEqual(routes.map(\.routeID), ["accepted"])
        XCTAssertNil(routes.first?.motorcycleAssessment)
    }
    func testCrossingRouteDoesNotContactSearchOrRequireRestrictionFallback() async throws {
        var calls = 0
        let routes = try await SDKRoutePlanner(policy: SDKMotorcyclePolicy()).routes(for: request()) { _, search in
            calls += 1; XCTAssertFalse(search)
            if calls > 1 { throw URLError(.timedOut) }
            return [self.crossing]
        }
        XCTAssertEqual(calls, 1)
        XCTAssertEqual(routes.first?.routeID, "crossing")
        XCTAssertNil(routes.first?.motorcycleAssessment)
    }

    private var elevated: [String: Any] {
        var route = outside
        route["sdk_has_elevated"] = true
        route["sdk_steps"] = [["road_name":"测试高架路", "link_type":4]]
        return route
    }
    func testElevatedCandidateTriggersCheckedExpresswayAlternative() async throws {
        var calls = 0
        let routes = try await SDKRoutePlanner(policy: SDKMotorcyclePolicy()).routes(for: request()) { parameters, search in
            calls += 1; XCTAssertFalse(search)
            XCTAssertEqual(parameters["strategy"] as? Int, calls == 1 ? 15 : 3)
            return calls == 1 ? [self.elevated] : [self.outside]
        }
        XCTAssertEqual(calls, 2)
        XCTAssertEqual(routes.first?.routeID, "accepted")
    }
    func testElevatedRoadAvoidanceReachesSearchAndFinalTypedRouteCheck() async throws {
        var calls = 0
        let routes = try await SDKRoutePlanner(policy: SDKMotorcyclePolicy()).routes(for: request()) { parameters, search in
            calls += 1
            if search {
                XCTAssertEqual(parameters["avoid_road"] as? String, "测试高架路")
                XCTAssertEqual((parameters["avoid_polygons"] as? [Any])?.count, 0)
                return [self.seed] // Search lacks link types and can only guide a new Navi calculation.
            }
            return calls < 5 ? [self.elevated] : [self.outside]
        }
        XCTAssertEqual(calls, 5)
        XCTAssertEqual(routes.first?.provider, "amap_ios_sdk")
    }
    func testUnavailableExpresswayStrategyStillAttemptsConstrainedSearch() async throws {
        var calls = 0
        let routes = try await SDKRoutePlanner(policy: SDKMotorcyclePolicy()).routes(for: request()) { parameters, search in
            calls += 1
            if calls == 1 { return [self.elevated] }
            if calls == 2 { throw AMapSDKError.message("No route for optional strategy") }
            if search {
                XCTAssertEqual(parameters["avoid_road"] as? String, "测试高架路")
                XCTAssertEqual((parameters["avoid_polygons"] as? [Any])?.count, 0)
                return [self.seed]
            }
            XCTAssertEqual(parameters["strategy"] as? Int, 15)
            return [self.outside]
        }
        XCTAssertEqual(calls, 4); XCTAssertEqual(routes.first?.routeID, "accepted")
    }
    func testCancelledOptionalAttemptDoesNotStartSearch() async {
        var calls = 0
        do {
            _ = try await SDKRoutePlanner(policy: SDKMotorcyclePolicy()).routes(for: request()) { _, search in
                calls += 1; XCTAssertFalse(search)
                if calls == 1 { return [self.elevated] }
                throw CancellationError()
            }
            XCTFail("Cancelled calculation was accepted")
        } catch { XCTAssertTrue(error is CancellationError) }
        XCTAssertEqual(calls, 2)
    }
    func testStillElevatedDetoursNeverReachPreview() async {
        var calls = 0
        do {
            _ = try await SDKRoutePlanner(policy: SDKMotorcyclePolicy()).routes(for: request()) { _, search in
                calls += 1; return search ? [self.seed] : [self.elevated]
            }
            XCTFail("Elevated route was accepted")
        } catch {
            XCTAssertTrue(error.localizedDescription.contains("路线偏好"))
            XCTAssertTrue(error.localizedDescription.contains("高架"))
        }
        XCTAssertEqual(calls, 5) // Only the explicit preferences trigger bounded detour attempts.
    }
    func testDisabledElevatedPreferenceAcceptsElevatedRouteWithoutFallback() async throws {
        var calls = 0
        let routes = try await SDKRoutePlanner(policy: SDKMotorcyclePolicy()).routes(for: request(avoidElevated: false)) { _, search in
            calls += 1; XCTAssertFalse(search); return [self.elevated]
        }
        XCTAssertEqual(calls, 1)
        XCTAssertEqual(routes.first?.provider, "amap_ios_sdk")
    }
    func testElevatedAndUnknownRoadTypesFailOnRerouteAndSearchSeedCannotStartNavigation() throws {
        let r = request(), policy = SDKMotorcyclePolicy()
        let reroute = RouteRequest(requestID: 3, origin: r.origin, destination: r.destination, isReroute: true,
                                   previousRouteID: "old", motorcycleOptions: r.motorcycleOptions)
        XCTAssertThrowsError(try policy.check(request: reroute, route: elevated))
        var unknown = outside; unknown.removeValue(forKey: "sdk_elevated_checked")
        XCTAssertThrowsError(try policy.check(request: reroute, route: unknown))
        XCTAssertThrowsError(try policy.check(request: r, route: seed))
        XCTAssertNotNil(try policy.check(request: r, route: seed, searchSeed: true))
    }
    func testRerouteAcrossFormerlyRestrictedAreaAndNamedTunnelNeedsNoAssessment() throws {
        let r = request()
        let reroute = RouteRequest(requestID: 3, origin: r.origin, destination: r.destination, isReroute: true,
                                   previousRouteID: "old", motorcycleOptions: r.motorcycleOptions)
        var raw = crossing
        raw["sdk_steps"] = [["road_name":"建宁路长江隧道", "link_type":2, "polyline":"118.73,32.1;118.74,32.11"]]
        let route = try XCTUnwrap(SDKMotorcyclePolicy().check(request: reroute, route: raw))
        XCTAssertNil(route.motorcycleAssessment)
        XCTAssertEqual(route.routeID, "crossing")
    }

    func testSearchRetainsOnlyManualAreasAndOrderedWaypoints() async throws {
        let manual = MotorcycleAvoidArea(id: "saved", name: "保留区域",
            points: [point(118.6,31.8),point(118.61,31.8),point(118.61,31.81)])
        let original = request()
        let r = RouteRequest(requestID: 10, origin: original.origin, destination: original.destination,
            motorcycleOptions: MotorcycleRouteOptions(avoidAreas: [manual], waypoints: [point(118.75,31.875)]))
        var calls = 0
        let routes = try await SDKRoutePlanner(policy: SDKMotorcyclePolicy()).routes(for: r) { parameters, search in
            calls += 1
            XCTAssertEqual((parameters["avoid_polygons"] as? [Any])?.count, 1)
            if search {
                XCTAssertEqual((parameters["waypoints"] as? [Any])?.count, 1)
                return [self.seed]
            }
            if calls < 3 { return [self.elevated] }
            return [self.crossing]
        }
        XCTAssertEqual(calls, 4)
        XCTAssertEqual(routes.first?.routeID, "crossing")
        XCTAssertNil(routes.first?.motorcycleAssessment)
    }
    func testCustomAreaEndpointStillStopsBeforeAnySDKCall() async {
        let original = request()
        let area = MotorcycleAvoidArea(id: "saved", name: "自定义禁区",
            points: [point(118.69,31.89),point(118.71,31.89),point(118.71,31.91),point(118.69,31.91)])
        let r = RouteRequest(requestID: 10, origin: original.origin, destination: original.destination,
            motorcycleOptions: MotorcycleRouteOptions(avoidAreas: [area]))
        var calls = 0
        do {
            _ = try await SDKRoutePlanner(policy: SDKMotorcyclePolicy()).routes(for: r) { _, _ in calls += 1; return [] }
            XCTFail("Manual area endpoint conflict was ignored")
        } catch { XCTAssertTrue(error.localizedDescription.contains("自定义")) }
        XCTAssertEqual(calls, 0)
    }
    func testRuntimeAcceptsRestrictedRerouteAndStillRejectsElevatedRoads() throws {
        let bridge = SDKRuntimeBridgeFixture()
        let service = AMapSDKService(bridge: bridge)
        let runtime = AMapSDKNavigationRuntime(service: service, request: request())
        defer { runtime.stop() }
        let initial = try XCTUnwrap(service.policy.check(request: request(), route: outside))
        var assessment: MotorcycleRouteAssessment?
        runtime.onRouteAssessment = { assessment = $0 }
        XCTAssertTrue(runtime.start(route: initial))
        XCTAssertTrue(bridge.approveRoute?(crossing) == true)
        XCTAssertNil(assessment)
        XCTAssertEqual(runtime.activeRoutePolyline.count, 3)
        XCTAssertFalse(bridge.approveRoute?(elevated) ?? true)
    }
    func testCoordinateRoundTripDoesNotDoubleShiftSDKPOI() {
        let original = GCJ02Point(longitudeDeg: 118.795, latitudeDeg: 32.041)
        let result = ChinaCoordinateTransform.wgs84ToGCJ02(ChinaCoordinateTransform.gcj02ToWGS84(original))
        XCTAssertEqual(result.longitudeDeg, original.longitudeDeg, accuracy: 0.00001)
        XCTAssertEqual(result.latitudeDeg, original.latitudeDeg, accuracy: 0.00001)
    }
}

private final class SDKRuntimeBridgeFixture: AMapSDKBridge {
    override func startRoute(_ routeID: String) -> Bool { true }
    override func stop() {}
}
