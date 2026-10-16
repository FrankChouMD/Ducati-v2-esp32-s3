import Foundation
import XCTest
@testable import MotoNavigationCore

final class MotorcycleRoutingTests: XCTestCase {
    func testUserSelectedSuburbanDefaultAndRequestRoundTrip() throws {
        let options = MotorcycleRouteOptions()
        XCTAssertEqual(options.registration, .nanjingSuburban)
        XCTAssertTrue(options.avoidHighways)
        let request = RouteRequest(requestID: 8, origin: point(118.7, 31.9), destination: point(118.9, 31.9),
                                   isReroute: true, previousRouteID: "old", motorcycleOptions: options)
        let data = try JSONEncoder().encode(request)
        let json = try XCTUnwrap(JSONSerialization.jsonObject(with: data) as? [String: Any])
        let applied = try XCTUnwrap(json["motorcycle_options"] as? [String: Any])
        XCTAssertEqual(applied["registration"] as? String, "nanjing_suburban")
        XCTAssertEqual(applied["avoid_highways"] as? Bool, true)
        XCTAssertEqual(try JSONDecoder().decode(RouteRequest.self, from: data), request)
        let legacy = RouteRequest(requestID: 9, origin: point(118.7,31.9), destination: point(118.9,31.9))
        let legacyJSON = try XCTUnwrap(JSONSerialization.jsonObject(with: JSONEncoder().encode(legacy)) as? [String: Any])
        XCTAssertNil(legacyJSON["motorcycle_options"])
    }

    func testConcaveVerticesAreRetainedAndSelfCrossingIsRejected() throws {
        let points = [point(118.70,31.90),point(118.73,31.90),point(118.73,31.91),
                      point(118.71,31.91),point(118.71,31.93),point(118.70,31.93)]
        let area = MotorcycleAvoidArea(name: "L 形", points: points)
        XCTAssertNil(area.validationMessage)
        let copy = try JSONDecoder().decode(MotorcycleAvoidArea.self, from: JSONEncoder().encode(area))
        XCTAssertEqual(copy.points, points)
        let crossing = MotorcycleAvoidArea(name: "交叉", points: [point(118.7,31.9),point(118.8,32),point(118.7,32),point(118.8,31.9)])
        XCTAssertNotNil(crossing.validationMessage)
    }

    func testWaypointArrivalOnlyConsumesNextOrderedPointAndPreservesPreferences() {
        let now = Date(timeIntervalSince1970: 1_700_000_000)
        let first = point(118.7,31.9), second = point(118.8,31.9)
        let options = MotorcycleRouteOptions(avoidTolls: true, waypoints: [first,second])
        let atSecond = NavigationFix(coordinate: second, horizontalAccuracyM: 3, timestamp: now)
        XCTAssertEqual(options.advancingWaypoints(with: atSecond, now: now), options)
        let good = NavigationFix(coordinate: first, horizontalAccuracyM: 3, timestamp: now)
        let remaining = options.advancingWaypoints(with: good, now: now)
        XCTAssertEqual(remaining.waypoints, [second])
        XCTAssertEqual(remaining.registration, .nanjingSuburban)
        XCTAssertTrue(remaining.avoidTolls)
        XCTAssertTrue(remaining.avoidHighways)
        let stale = NavigationFix(coordinate: first, horizontalAccuracyM: 3, timestamp: now.addingTimeInterval(-20))
        XCTAssertEqual(options.advancingWaypoints(with: stale, now: now), options)
        let poor = NavigationFix(coordinate: first, horizontalAccuracyM: 100, timestamp: now)
        XCTAssertEqual(options.advancingWaypoints(with: poor, now: now), options)
    }

    private func point(_ lon: Double, _ lat: Double) -> WGS84Point {
        WGS84Point(longitudeDeg: lon, latitudeDeg: lat)
    }

    func testAreaAssessmentMustIncludeMatchingSourceGeometryAndProfile() throws {
        let options = MotorcycleRouteOptions()
        var json: [String: Any] = ["policy_version":"nanjing-test","verified_on":"2026-10-16",
            "coverage":"partial","status":"review_required","checked_at_ms":0,
            "applied_options":try JSONSerialization.jsonObject(with:JSONEncoder().encode(options)),
            "warnings":["Partial coverage"],"sources":[["title":"Motu","url":"https://www.motumap.com/"]]]
        func decode(_ json: [String: Any]) throws -> MotorcycleRouteAssessment {
            try JSONDecoder().decode(MotorcycleRouteAssessment.self, from:JSONSerialization.data(withJSONObject:json))
        }
        XCTAssertFalse(try decode(json).matches(options)) // Old gateway with no area check.
        var area: [String: Any] = ["id":"motu-025-10255","name":"Nanjing","coordinate_system":"GCJ-02",
            "source_url":"https://www.motumap.com/nodata.js","data_version":"motu-025-10255-test",
            "applies_to_registration":true,
            "polygon":[["longitude_deg":118.7,"latitude_deg":32.0],
                       ["longitude_deg":118.8,"latitude_deg":32.0],
                       ["longitude_deg":118.7,"latitude_deg":32.1]]]
        json["restriction_areas"] = [area]; json["camera_data_status"] = "unavailable"
        let checked = try decode(json)
        XCTAssertTrue(checked.matches(options))
        XCTAssertEqual(checked.restrictionAreas?.first?.polygon.count,3)
        XCTAssertEqual(checked.cameraDataStatus,"unavailable")
        area["applies_to_registration"] = false; json["restriction_areas"] = [area]
        XCTAssertFalse(try decode(json).matches(options))
        area["applies_to_registration"] = true; area["coordinate_system"] = "WGS84"
        json["restriction_areas"] = [area]
        XCTAssertFalse(try decode(json).matches(options))
    }
}
