import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";
import { MOTU_NANJING as region } from "../../shared/motorcycle/nanjing-motu-10255.v1.js";
import { gcj02ToWgs84, wgs84ToGcj02 } from "../src/coordinates.js";
import { buildAmapDrivingUrl, createAmapProvider } from "../src/amap-provider.js";
import { assessMotorcycleRoute, pointInPolygon, polylineHitsPolygon } from "../src/motorcycle-policy.js";

const point = (longitude_deg, latitude_deg) => ({ coordinate_system: "WGS84", longitude_deg, latitude_deg });
const options = registration => ({ region: "nanjing", registration, avoid_highways: true,
  avoid_tolls: false, avoid_ferries: false, avoid_areas: [], waypoints: [] });
const request = (registration = "nanjing_suburban") => ({ protocol_version: 1, request_id: 12,
  route_mode: "driving", origin: point(118.7, 31.9), destination: point(118.9, 31.9),
  is_reroute: false, motorcycle_options: options(registration) });
const center = { longitude_deg: 118.8, latitude_deg: 32.05 };
const centerWgs84 = gcj02ToWgs84(center);
const crossing = [[118.6, 32.05], [119.0, 32.05]];
const outside = [[118.6, 31.9], [118.9, 31.9]];
const rawPath = line => ({ steps: [{ polyline: line.map(p => p.join(",")).join(";") }] });
const route = line => ({ polyline: line.map(([longitude_deg, latitude_deg]) => ({ longitude_deg, latitude_deg })) });
const areaM2 = ring => {
  const local = ring.map(([x, y]) => [(x - ring[0][0]) * 111_320 * Math.cos(32 * Math.PI / 180),
    (y - ring[0][1]) * 111_320]);
  return Math.abs(local.reduce((sum, p, i) => {
    const q = local[(i + 1) % local.length]; return sum + p[0] * q[1] - q[0] * p[1];
  }, 0)) / 2;
};

test("pinned 124-point area is decomposed within provider limits without filling concave gaps", () => {
  assert.equal(region.polygon.length, 124);
  assert.equal(region.avoid_polygons.length, 28);
  const sourceVertices = new Set(region.polygon.map(JSON.stringify));
  for (const ring of region.avoid_polygons) {
    assert.ok(ring.length >= 3 && ring.length <= 16);
    assert.ok(areaM2(ring) <= 80_000_000);
    assert.ok(ring.every(p => sourceVertices.has(JSON.stringify(p))));
  }
  assert.ok(Math.abs(region.avoid_polygons.reduce((sum, r) => sum + areaM2(r), 0) - areaM2(region.polygon)) < 0.001);
  // Independent grid comparison covers both the inside and the concave outside
  // corners, rather than testing only vertices used by the partition generator.
  for (let x = 0; x <= 60; x++) for (let y = 0; y <= 60; y++) {
    const p = [118.60 + x * 0.005, 31.90 + y * 0.005];
    assert.equal(region.avoid_polygons.some(r => pointInPolygon(p, r)), pointInPolygon(p, region.polygon), p.join(","));
  }
});

test("suburban, external and unknown profiles send the source GCJ-02 values; urban profiles do not", () => {
  for (const registration of ["nanjing_suburban", "non_nanjing", "unknown"]) {
    const url = buildAmapDrivingUrl(request(registration), { key: "test" });
    const pieces = url.searchParams.get("avoidpolygons").split("|");
    assert.equal(pieces.length, 28);
    assert.equal(pieces[0].split(";")[0], region.avoid_polygons[0][0].map(v => v.toFixed(6)).join(","));
    assert.ok(url.href.length < 7000);
  }
  assert.equal(buildAmapDrivingUrl(request("nanjing_urban"), { key: "test" }).searchParams.has("avoidpolygons"), false);
});

test("four manual WGS84 areas share the 32-region budget without dropping the Motu area", () => {
  const r = request();
  r.motorcycle_options.avoid_areas = Array.from({ length: 4 }, (_, i) => ({ id: `manual-${i}`, name: `Manual ${i}`,
    points: Array.from({ length: 16 }, (_, k) => point(
      118.7 + i * 0.001 + 0.002 * Math.cos(k * Math.PI / 8),
      31.88 + 0.002 * Math.sin(k * Math.PI / 8))) }));
  const url = buildAmapDrivingUrl(r, { key: "test" });
  const areas = url.searchParams.get("avoidpolygons").split("|");
  assert.equal(areas.length, 32);
  const manual = wgs84ToGcj02(r.motorcycle_options.avoid_areas[0].points[0]);
  assert.equal(areas[28].split(";")[0], `${manual.longitude_deg.toFixed(6)},${manual.latitude_deg.toFixed(6)}`);
  assert.ok(url.href.length < 8000);
});

test("origin, destination and ordered waypoint conflicts fail before contacting the upstream", async () => {
  let calls = 0;
  const provider = createAmapProvider({ key: "test", fetchImpl: async () => { calls++; throw new Error("unexpected"); } });
  for (const target of ["origin", "destination", "waypoint"]) {
    const r = request();
    if (target === "waypoint") r.motorcycle_options.waypoints = [centerWgs84];
    else r[target] = centerWgs84;
    await assert.rejects(provider.planRoute(r), error => error.code === "MOTORCYCLE_AREA_CONFLICT" && !error.retryable);
  }
  assert.equal(calls, 0);
});

test("crossing routes and boundary contact are blocked even with outside endpoints and no road name", () => {
  assert.ok(crossing.every(p => !pointInPolygon(p, region.polygon)));
  assert.ok(polylineHitsPolygon(crossing, region.polygon));
  for (const registration of ["nanjing_suburban", "non_nanjing", "unknown"]) {
    assert.match(assessMotorcycleRoute(route(crossing), rawPath(crossing), request(registration)).violations[0], /摩途/);
    const edge = region.polygon.slice(0, 2);
    assert.match(assessMotorcycleRoute(route(edge), rawPath(edge), request(registration)).violations[0], /摩途/);
  }
  const urban = assessMotorcycleRoute(route(crossing), rawPath(crossing), request("nanjing_urban"));
  assert.deepEqual(urban.violations, []);
  assert.equal(urban.assessment.restriction_areas[0].applies_to_registration, false);
});

test("full upstream geometry is checked independently of the presentation polyline", () => {
  const result = assessMotorcycleRoute(route(outside), rawPath([outside[0], [center.longitude_deg, center.latitude_deg], outside[1]]), request());
  assert.match(result.violations[0], /摩途/);
  assert.deepEqual(assessMotorcycleRoute(route(outside), rawPath(outside), request()).violations, []);
  const incomplete = { steps: [{ polyline: "118.6,31.9;118.9,31.9" }, {}],
    polyline: "118.6,32.05;119.0,32.05" };
  assert.match(assessMotorcycleRoute(route(crossing), incomplete, request()).violations[0], /摩途/);
});

async function payload(lines) {
  const fixture = JSON.parse(await readFile(new URL("../fixtures/amap-route-v2.json", import.meta.url), "utf8"));
  const baseline = fixture.route.paths[0];
  fixture.route.paths = lines.map((line, index) => ({ ...baseline, path_id: `nanjing-${index}`,
    steps: [{ ...baseline.steps[0], road_name: "未命名道路", polyline: rawPath(line).steps[0].polyline }] }));
  return fixture;
}

test("candidate filtering returns the outside route with the actual area and unavailable camera status", async () => {
  const provider = createAmapProvider({ key: "test", fetchImpl: async () => Response.json(await payload([crossing, outside])) });
  const result = await provider.planRoute(request());
  assert.match(result.route_id, /nanjing-1/);
  const assessment = result.motorcycle_assessment;
  assert.equal(assessment.restriction_areas[0].polygon.length, 124);
  assert.equal(assessment.restriction_areas[0].data_version, region.data_version);
  assert.equal(assessment.restriction_areas[0].applies_to_registration, true);
  assert.equal(assessment.camera_data_status, "unavailable");
  assert.equal(assessment.coverage, "partial");
});

test("reroute retry retains all areas and refuses a still-crossing result", async () => {
  const seen = [];
  const provider = createAmapProvider({ key: "test", fetchImpl: async url => {
    seen.push(new URL(url)); return Response.json(await payload([crossing]));
  } });
  const r = request(); r.is_reroute = true; r.previous_route_id = "previous";
  await assert.rejects(provider.planRoute(r), error => error.code === "MOTORCYCLE_ROUTE_BLOCKED" && !error.retryable);
  assert.equal(seen.length, 2);
  assert.equal(seen[0].searchParams.get("avoidpolygons"), seen[1].searchParams.get("avoidpolygons"));
  assert.equal(seen[1].searchParams.get("avoidpolygons").split("|").length, 28);
});

test("upstream parameter refusal cannot fall back to a request without area constraints", async () => {
  let calls = 0;
  const provider = createAmapProvider({ key: "test", fetchImpl: async () => {
    calls++; return Response.json({ status: "0", infocode: "20000", info: "INVALID_PARAMS" });
  } });
  await assert.rejects(provider.planRoute(request()), error => error.code === "AMAP_20000" && !error.retryable);
  assert.equal(calls, 1);
});
