import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { once } from "node:events";
import test from "node:test";
import { createGateway } from "../src/gateway.js";
import { buildAmapDrivingUrl, createAmapProvider } from "../src/amap-provider.js";
import { wgs84ToGcj02 } from "../src/coordinates.js";
import { validateRouteRequest } from "../src/validation.js";
import { assessMotorcycleRoute, NANJING_POLICY, pointInPolygon, polylineHitsPolygon, validateMotorcycleOptions } from "../src/motorcycle-policy.js";

const point = (longitude_deg, latitude_deg) => ({ coordinate_system: "WGS84", longitude_deg, latitude_deg });
const settings = () => ({region: "nanjing", registration: "nanjing_suburban", avoid_highways: true,
  avoid_tolls: false, avoid_ferries: false, avoid_areas: [], waypoints: []});
const request = () => ({protocol_version: 1, request_id: 7, route_mode: "driving", is_reroute: false,
  origin: point(118.7, 31.9), destination: point(118.9, 31.9), motorcycle_options: settings()});
const fixture = () => readFile(new URL("../fixtures/amap-route-v2.json", import.meta.url), "utf8").then(JSON.parse);
const response = payload => new Response(JSON.stringify(payload), {headers: {"Content-Type": "application/json"}});
const area = () => ({id: "test-area", name: "自用避让区", points: [point(118.75, 31.89), point(118.76, 31.89), point(118.76, 31.91), point(118.75, 31.91)]});
const minimalRoute = {polyline: [{longitude_deg: 118.8, latitude_deg: 31.9}, {longitude_deg: 118.81, latitude_deg: 31.91}]};
const namedPath = name => ({steps: [{road_name: name, cities: [{city: "南京市", adcode: "320100"}]}]});

test("ordinary clients stay at strategy 32; Nanjing settings select documented car strategies", () => {
  const r = request();
  const url = () => buildAmapDrivingUrl(r, {key: "test"});
  assert.equal(url().searchParams.get("strategy"), "40");
  assert.equal(url().searchParams.get("ferry"), "0");
  r.motorcycle_options.avoid_tolls = true;
  assert.equal(url().searchParams.get("strategy"), "43");
  r.motorcycle_options.avoid_highways = false;
  assert.equal(url().searchParams.get("strategy"), "41");
  r.motorcycle_options.avoid_tolls = false;
  r.motorcycle_options.avoid_ferries = true;
  assert.equal(url().searchParams.get("strategy"), "33");
  assert.equal(url().searchParams.get("ferry"), "1");
  assert.equal(url().searchParams.has("cartype"), false);
  delete r.motorcycle_options;
  assert.equal(url().searchParams.get("strategy"), "32");
});

test("manual vertices and ordered waypoints cross WGS84 to GCJ-02 exactly once without changing the request", () => {
  const r = request(); r.motorcycle_options.avoid_areas = [area()];
  r.motorcycle_options.waypoints = [point(118.71, 31.92), point(118.72, 31.93)];
  const before = structuredClone(r);
  const params = buildAmapDrivingUrl(r, {key: "test"}).searchParams;
  const converted = wgs84ToGcj02(area().points[0]);
  assert.equal(params.get("avoidpolygons").split("|").at(-1).split(";")[0], `${converted.longitude_deg.toFixed(6)},${converted.latitude_deg.toFixed(6)}`);
  assert.equal(params.get("waypoints").split(";").length, 2);
  assert.deepEqual(r, before);
});

test("full route segments crossing a polygon are rejected even when both endpoints are outside", () => {
  const ring = [[0,0],[2,0],[2,2],[0,2]];
  assert.equal(polylineHitsPolygon([[-1,1],[3,1]], ring), true);
  assert.equal(polylineHitsPolygon([[-1,0],[0,0]], ring), true);
  assert.equal(polylineHitsPolygon([[-1,3],[3,3]], ring), false);
});

test("concave L polygon keeps its empty corner", () => {
  const ring = [[0,0],[3,0],[3,1],[1,1],[1,3],[0,3]];
  assert.equal(pointInPolygon([2,2], ring), false);
  assert.equal(polylineHitsPolygon([[1.5,2],[2.5,2]], ring), false);
  assert.equal(polylineHitsPolygon([[-1,2],[2,2]], ring), true);
});

test("malformed, self-crossing, oversized and duplicate areas cannot silently disable avoidance", () => {
  for (const mutate of [
    o => {o.registration = "苏A";}, o => {o.avoid_highways = "true";},
    o => {o.strategy = 11;}, o => {o.waypoints = [point(181,32)];},
    o => {o.avoid_areas = [{...area(), points: [point(118.75,31.89),point(118.76,31.91),point(118.75,31.91),point(118.76,31.89)]}];},
    o => {o.avoid_areas = [{...area(), points: [point(118,31),point(120,31),point(120,33),point(118,33)]}];},
    o => {o.avoid_areas = [area(),area()];},
    o => {o.avoid_areas = [{...area(), extra: true}];},
  ]) {
    const options = settings(); mutate(options);
    assert.throws(() => validateMotorcycleOptions(options), error => error.code === "INVALID_REQUEST");
  }
  const r = request(); r.motorcycle_options.avoid_areas = [area()];
  r.motorcycle_options.waypoints = [point(118.755,31.9)];
  assert.throws(() => validateRouteRequest(r), /途经点/);
});

test("suburban registration is not treated as urban permission on the old Yangtze Bridge", () => {
  const r = request();
  assert.equal(assessMotorcycleRoute(minimalRoute, namedPath("南京长江大桥"), r).violations.length, 1);
  r.motorcycle_options.registration = "nanjing_urban";
  const result = assessMotorcycleRoute(minimalRoute, namedPath("南京长江大桥"), r);
  assert.equal(result.violations.length, 0);
  assert.equal(result.assessment.status, "review_required");
  assert.equal(result.assessment.coverage, "partial");
});

test("known crossings are blocked for every registration; approaching street names are not substring matches", () => {
  for (const registration of ["nanjing_urban","nanjing_suburban","non_nanjing","unknown"]) {
    const r = request(); r.motorcycle_options.registration = registration;
    for (const name of ["新生圩长江大桥", "建宁路长江隧道", "南京长江二桥", "应天大街长江隧道"]) {
      assert.equal(assessMotorcycleRoute(minimalRoute, namedPath(name), r).violations.length, 1, `${registration}/${name}`);
    }
    assert.equal(assessMotorcycleRoute(minimalRoute, namedPath("长江大桥北路"), r).violations.length, 0);
  }
  assert.equal(assessMotorcycleRoute(minimalRoute, {steps:[{road_name:"长江大桥", polyline:"114.28,30.55;114.29,30.56"}]}, request()).violations.length, 0);
});

test("manual avoidance is checked against the entire transformed polyline", () => {
  const r = request(); r.motorcycle_options.avoid_areas = [area()];
  const route = {polyline: [point(118.74,31.9),point(118.77,31.9)].map(wgs84ToGcj02)};
  const result = assessMotorcycleRoute(route, {steps:[]}, r);
  assert.match(result.violations[0], /自用避让区/);
});

test("a rejected first candidate can never be the single-route response", async () => {
  const payload = await fixture();
  const allowed = structuredClone(payload.route.paths[0]); allowed.path_id = "allowed";
  payload.route.paths[0].steps[0].road_name = "建宁路长江隧道";
  payload.route.paths[0].steps[0].cities = [{city:"南京市"}];
  payload.route.paths.push(allowed);
  const provider = createAmapProvider({key:"test", fetchImpl: async () => response(payload)});
  const route = await provider.planRoute(request());
  assert.match(route.route_id, /allowed/);
  assert.deepEqual(route.motorcycle_assessment.applied_options, settings());
  assert.equal(route.motorcycle_assessment.policy_version, NANJING_POLICY.version);
});

test("bounded policy retry and reroute preserve restrictions instead of falling back to car mode", async () => {
  const payload = await fixture();
  payload.route.paths[0].steps[0].road_name = "新生圩长江大桥";
  payload.route.paths[0].steps[0].cities = [{adcode:"320100"}];
  const urls = [];
  const provider = createAmapProvider({key:"test", fetchImpl: async url => {urls.push(new URL(url)); return response(payload);}});
  const r = request(); r.is_reroute = true; r.previous_route_id = "previous";
  r.motorcycle_options.avoid_areas = [area()];
  r.motorcycle_options.waypoints = [point(118.72,31.9)];
  await assert.rejects(provider.planRoute(r), error => error.code === "MOTORCYCLE_ROUTE_BLOCKED" && error.retryable === false);
  assert.deepEqual(urls.map(u => u.searchParams.get("strategy")), ["40","35"]);
  assert.equal(urls[0].searchParams.get("avoidpolygons"), urls[1].searchParams.get("avoidpolygons"));
  assert.equal(urls[0].searchParams.get("waypoints"), urls[1].searchParams.get("waypoints"));
});

test("an endpoint inside an avoidance area fails before contacting the upstream", async () => {
  let calls = 0;
  const provider = createAmapProvider({key:"test", fetchImpl: async () => {calls++; throw new Error("unexpected");}});
  const r = request(); r.motorcycle_options.avoid_areas = [area()]; r.origin = point(118.755,31.9);
  await assert.rejects(provider.planRoute(r), error => error.code === "AVOIDANCE_ENDPOINT_CONFLICT");
  assert.equal(calls,0);
});

test("gateway returns partial coverage and exact applied settings on both route endpoints", async t => {
  const payload = await fixture();
  const provider = createAmapProvider({key:"test", fetchImpl: async () => response(payload)});
  const server = createGateway({provider, providerMode:"amap"});
  server.listen(0,"127.0.0.1"); await once(server,"listening");
  t.after(() => {server.closeAllConnections(); server.close();});
  const base = `http://127.0.0.1:${server.address().port}`;
  for (const path of ["/v1/routes","/v1/route-options"]) {
    const res = await fetch(base + path,{method:"POST",body:JSON.stringify(request()),headers:{"Content-Type":"application/json"}});
    assert.equal(res.status,200);
    const data = await res.json(), route = data.route ?? data.routes[0];
    assert.equal(route.motorcycle_assessment.coverage,"partial");
    assert.equal(route.motorcycle_assessment.status,"review_required");
    assert.deepEqual(route.motorcycle_assessment.applied_options,settings());
  }
  const health = await (await fetch(base+"/healthz")).json();
  assert.equal(health.capabilities.motorcycle_rules,true);
});
