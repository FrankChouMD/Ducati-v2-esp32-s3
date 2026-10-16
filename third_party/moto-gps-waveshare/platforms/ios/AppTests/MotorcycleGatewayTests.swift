import Foundation
import MotoNavigationCore
import XCTest
@testable import MOTO_GPS

final class MotorcycleGatewayTests: XCTestCase {
    func testLegacyPreferencesEnableElevatedAvoidanceWithoutLosingExistingChoices() throws {
        let suite = "MotorcycleMigration.\(UUID().uuidString)"
        let defaults = UserDefaults(suiteName: suite)!
        defer { defaults.removePersistentDomain(forName: suite) }
        let area = MotorcycleAvoidArea(id: "saved", name: "保留区域", points: [point(118.6,31.8),point(118.61,31.8),point(118.61,31.81)])
        let original = MotorcycleRouteOptions(registration: .nonNanjing, avoidHighways: false,
            avoidElevatedRoads: true, avoidTolls: true, avoidFerries: true,
            avoidAreas: [area], waypoints: [point(118.75,31.9)])
        var stored = try XCTUnwrap(JSONSerialization.jsonObject(with: JSONEncoder().encode(original)) as? [String: Any])
        stored.removeValue(forKey: "avoid_elevated_roads")
        defaults.set(try JSONSerialization.data(withJSONObject: stored), forKey: MotorcyclePreferenceStore.key)
        XCTAssertEqual(MotorcyclePreferenceStore.load(defaults: defaults), original)
        let migrated = try JSONDecoder().decode(MotorcycleRouteOptions.self, from: XCTUnwrap(defaults.data(forKey: MotorcyclePreferenceStore.key)))
        XCTAssertEqual(migrated, original)
        var disabled = original; disabled.avoidElevatedRoads = false
        try MotorcyclePreferenceStore.save(disabled, defaults: defaults)
        XCTAssertEqual(MotorcyclePreferenceStore.load(defaults: defaults), disabled)
    }

    func testPreviewRerouteAndTrafficSendTheirExactSettings() async throws {
        let configuration = URLSessionConfiguration.ephemeral
        configuration.protocolClasses = [MotorcycleTestURLProtocol.self]
        let session = URLSession(configuration: configuration)
        defer { session.invalidateAndCancel(); MotorcycleTestURLProtocol.handler = nil }
        let options = MotorcycleRouteOptions(avoidTolls: true, waypoints: [point(118.71,31.9),point(118.72,31.9)])
        let provider = AmapGatewayRouteProvider(baseURL: URL(string: "https://gateway.example.test/")!, session: session, motorcycleOptions: options)
        MotorcycleTestURLProtocol.handler = { request in
            let sent = try JSONDecoder().decode(RouteRequest.self, from: Self.body(request))
            XCTAssertNotNil(sent.motorcycleOptions)
            let assessment: [String: Any] = [
                "policy_version": "nanjing-test", "verified_on": "2026-10-16", "coverage": "partial",
                "status": "review_required", "checked_at_ms": 0,
                "applied_options": try JSONSerialization.jsonObject(with: JSONEncoder().encode(sent.motorcycleOptions!)),
                "warnings": ["未完整覆盖禁行范围"], "sources": [["title":"测试来源","url":"https://example.test/policy"]],
                "restriction_areas": [Self.area(sent.motorcycleOptions!)], "camera_data_status": "unavailable",
            ]
            let route = RoutePlan(routeID: "checked", provider: "amap", generatedAtMs: 0, totalDistanceM: 100,
                totalDurationS: 20, polyline: [GCJ02Point(longitudeDeg: 118.71, latitudeDeg:31.9),GCJ02Point(longitudeDeg:118.72,latitudeDeg:31.9)], maneuvers: [], traffic: [])
            var json = try XCTUnwrap(JSONSerialization.jsonObject(with: JSONEncoder().encode(route)) as? [String: Any])
            json["motorcycle_assessment"] = assessment
            if request.url?.path == "/v1/route-options" {
                return try JSONSerialization.data(withJSONObject: ["protocol_version":1,"request_id":sent.requestID,"routes":[json]])
            }
            return try JSONSerialization.data(withJSONObject: ["protocol_version":1,"request_id":sent.requestID,"route":json])
        }
        let initial = RouteRequest(requestID:1,origin:point(118.7,31.9),destination:point(118.8,31.9))
        let previews = try await provider.routeOptions(for: initial)
        XCTAssertEqual(previews.first?.motorcycleAssessment?.appliedOptions, options)
        var remaining = options; remaining.waypoints.removeFirst()
        let reroute = RouteRequest(requestID:2,origin:point(118.71,31.9),destination:point(118.8,31.9),
            isReroute:true,previousRouteID:"checked",motorcycleOptions:remaining)
        let rerouted = try await provider.route(for: reroute)
        XCTAssertEqual(rerouted.route.motorcycleAssessment?.appliedOptions, remaining)
        let traffic = RouteRequest(requestID:3,origin:point(118.7,31.9),destination:point(118.8,31.9),
            previousRouteID:"checked",motorcycleOptions:options)
        let refreshed = try await provider.route(for: traffic)
        XCTAssertEqual(refreshed.route.motorcycleAssessment?.appliedOptions, options)
    }

    func testOldGatewayCannotSilentlyReturnAnUncheckedCarRoute() async throws {
        let configuration = URLSessionConfiguration.ephemeral
        configuration.protocolClasses = [MotorcycleTestURLProtocol.self]
        let session = URLSession(configuration: configuration)
        defer { session.invalidateAndCancel(); MotorcycleTestURLProtocol.handler = nil }
        MotorcycleTestURLProtocol.handler = { _ in
            let route = RoutePlan(routeID:"unchecked",provider:"amap",generatedAtMs:0,totalDistanceM:100,
                totalDurationS:20,polyline:[],maneuvers:[],traffic:[])
            return try JSONEncoder().encode(RouteEnvelope(requestID:1,route:route))
        }
        let provider = AmapGatewayRouteProvider(baseURL: URL(string:"https://gateway.example.test/")!,session:session,motorcycleOptions:MotorcycleRouteOptions())
        do {
            _ = try await provider.route(for: RouteRequest(requestID:1,origin:point(118.7,31.9),destination:point(118.8,31.9)))
            XCTFail("An unchecked route must not be accepted")
        } catch let error as AmapGatewayError {
            XCTAssertTrue(error.stopsMotorcycleGuidance)
            XCTAssertFalse(error.isRetryable)
        }
    }

    private func point(_ lon: Double, _ lat: Double) -> WGS84Point {
        WGS84Point(longitudeDeg:lon,latitudeDeg:lat)
    }

    func testDriftCannotSilentlyReplaceTheReviewedMotorcyclePreview() async throws {
        let options = MotorcycleRouteOptions()
        let json: [String: Any] = ["policy_version":"nanjing-test","verified_on":"2026-10-16",
            "coverage":"partial","status":"review_required","checked_at_ms":0,
            "applied_options":try JSONSerialization.jsonObject(with:JSONEncoder().encode(options)),
            "warnings":["覆盖不完整"],"sources":[["title":"测试","url":"https://example.test/policy"]],
            "restriction_areas":[Self.area(options)],"camera_data_status":"unavailable"]
        let assessment = try JSONDecoder().decode(MotorcycleRouteAssessment.self,from:JSONSerialization.data(withJSONObject:json))
        XCTAssertTrue(assessment.matches(options))
        var different = options; different.registration = .nanjingUrban
        XCTAssertFalse(assessment.matches(different))
        let route = RoutePlan(routeID:"reviewed",provider:"amap",generatedAtMs:0,totalDistanceM:100,
            totalDurationS:20,polyline:[],maneuvers:[],traffic:[],motorcycleAssessment:assessment)
        let provider = PreviewSelectedRouteProvider(selectedRoute:route,selectedRouteOrigin:point(118.7,31.9),
            liveProvider:MotorcycleUnexpectedProvider())
        do {
            _ = try await provider.route(for:RouteRequest(requestID:1,origin:point(118.8,31.9),destination:point(118.9,31.9)))
            XCTFail("A different origin requires a new preview")
        } catch let error as AmapGatewayError {
            if case .motorcyclePreviewChanged = error {} else { XCTFail("Unexpected policy error") }
            XCTAssertTrue(error.stopsMotorcycleGuidance)
        }
    }
    private static func body(_ request: URLRequest) -> Data {
        if let data = request.httpBody { return data }
        guard let stream = request.httpBodyStream else { return Data() }
        stream.open(); defer { stream.close() }
        var data = Data(), buffer = [UInt8](repeating:0,count:1024)
        while stream.hasBytesAvailable {
            let length = stream.read(&buffer,maxLength:buffer.count)
            if length <= 0 { break }
            data.append(contentsOf:buffer.prefix(length))
        }
        return data
    }

    private static func area(_ options: MotorcycleRouteOptions) -> [String: Any] {
        ["id":"motu-025-10255","name":"南京禁外摩区域","coordinate_system":"GCJ-02",
         "source_url":"https://www.motumap.com/nodata.js","data_version":"motu-025-10255-test",
         "applies_to_registration":options.registration != .nanjingUrban,
         "polygon":[["longitude_deg":118.7,"latitude_deg":32.0],
                    ["longitude_deg":118.8,"latitude_deg":32.0],
                    ["longitude_deg":118.7,"latitude_deg":32.1]]]
    }

    func testPermanentUpstreamRefusalAndAreaConflictStopGuidance() {
        XCTAssertTrue(AmapGatewayError.rejected(code:"AMAP_20000",message:"Invalid avoidance parameters",retryable:false).stopsMotorcycleGuidance)
        XCTAssertTrue(AmapGatewayError.rejected(code:"MOTORCYCLE_AREA_CONFLICT",message:"Area conflict",retryable:false).stopsMotorcycleGuidance)
        XCTAssertFalse(AmapGatewayError.rejected(code:"PROVIDER_TIMEOUT",message:"Timeout",retryable:true).stopsMotorcycleGuidance)
    }
}

private struct MotorcycleUnexpectedProvider: NavigationRouteProviding {
    func route(for request: RouteRequest) async throws -> RouteEnvelope { throw URLError(.badServerResponse) }
}

private final class MotorcycleTestURLProtocol: URLProtocol {
    nonisolated(unsafe) static var handler: ((URLRequest) throws -> Data)?
    override class func canInit(with request: URLRequest) -> Bool { request.url?.host == "gateway.example.test" }
    override class func canonicalRequest(for request: URLRequest) -> URLRequest { request }
    override func startLoading() {
        do {
            guard let handler = Self.handler else { throw URLError(.badServerResponse) }
            let data = try handler(request)
            let response = HTTPURLResponse(url:request.url!,statusCode:200,httpVersion:nil,headerFields:["Content-Type":"application/json"])!
            client?.urlProtocol(self,didReceive:response,cacheStoragePolicy:.notAllowed)
            client?.urlProtocol(self,didLoad:data)
            client?.urlProtocolDidFinishLoading(self)
        } catch { client?.urlProtocol(self,didFailWithError:error) }
    }
    override func stopLoading() {}
}
