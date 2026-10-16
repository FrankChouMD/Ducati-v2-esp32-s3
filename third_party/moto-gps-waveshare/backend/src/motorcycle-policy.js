// Ducati V2 personal adaptation, 2026-10-16. This is a partial rule checker
// around ordinary car routing, not a licensed motorcycle routing engine.
import { wgs84ToGcj02 } from "./coordinates.js";
import { MOTU_NANJING } from "../../shared/motorcycle/nanjing-motu-10255.v1.js";

export const NANJING_POLICY = Object.freeze({
  version: "nanjing-2026-10-16.2",
  verified_on: "2026-10-16",
  coverage: "partial",
  sources: [
    { title: "摩途官网南京禁外摩图层（第三方数据）", url: "https://www.motumap.com/" },
    { title: "南京市政府禁外摩通告与附图", url: "https://www.nanjing.gov.cn/xxgkn/szgfxwj/202311/t20231115_4098444.html" },
    { title: "南京禁外摩范围与适用车辆", url: "https://www.nanjing.gov.cn/zdgk/200805/t20080528_1054634.html" },
    { title: "南京市交通运输局过江通道答复", url: "https://www.nanjing.gov.cn/xxgkn/jytabljggk/2025njytabl/shirddbjy/202512/t20251205_5706046.html" },
    { title: "新生圩长江大桥通行政策", url: "https://jtj.nanjing.gov.cn/njsjtysj/202511/t20251121_5695260.html" },
    { title: "建宁路长江隧道通行政策", url: "https://www.nanjing.gov.cn/hdjl/hygq/202608/t20260819_5896120.html" },
  ],
});

// These are exact road-name aliases, not guessed polygons or district boxes.
// The 2025 Transport Bureau reply covers existing crossings except the old
// Nanjing Yangtze Bridge. New crossings have their own specific notices.
const CROSSINGS = [
  ["八卦洲长江大桥", "南京长江二桥", "长江二桥"],
  ["大胜关长江大桥", "南京长江三桥", "长江三桥"],
  ["栖霞山长江大桥", "南京长江四桥", "长江四桥"],
  ["江心洲长江大桥", "南京长江五桥", "长江五桥", "江心洲大桥"],
  ["应天大街长江隧道", "南京长江隧道", "应天大街隧道"],
  ["定淮门长江隧道", "南京扬子江隧道", "扬子江隧道", "定淮门隧道"],
  ["燕子矶长江隧道", "燕子矶隧道"],
  ["新生圩长江大桥"],
  ["建宁路长江隧道"],
];
const OLD_BRIDGE = ["南京长江大桥", "长江大桥"];
const REGISTRATIONS = new Set(["nanjing_suburban", "nanjing_urban", "non_nanjing", "unknown"]);
const KEYS = new Set(["region", "registration", "avoid_highways", "avoid_tolls", "avoid_ferries", "avoid_areas", "waypoints"]);
const EPS = 1e-12;

export class MotorcyclePolicyError extends Error {
  constructor(code, message) {
    super(message);
    this.code = code;
    this.retryable = false;
  }
}

function invalid(message) { throw new MotorcyclePolicyError("INVALID_REQUEST", message); }
function object(value) { return value && typeof value === "object" && !Array.isArray(value); }
function onlyKeys(value, keys) { return Object.keys(value).every(key => keys.has(key)); }
function point(p) {
  return object(p) && onlyKeys(p, new Set(["coordinate_system", "longitude_deg", "latitude_deg"])) &&
    p.coordinate_system === "WGS84" && Number.isFinite(p.longitude_deg) &&
    Number.isFinite(p.latitude_deg) && Math.abs(p.longitude_deg) <= 180 && Math.abs(p.latitude_deg) <= 85;
}
const xy = p => [p.longitude_deg, p.latitude_deg];
const cross = (a, b, c) => (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
function onSegment(a, b, p) {
  return Math.abs(cross(a, b, p)) <= EPS &&
    p[0] >= Math.min(a[0], b[0]) - EPS && p[0] <= Math.max(a[0], b[0]) + EPS &&
    p[1] >= Math.min(a[1], b[1]) - EPS && p[1] <= Math.max(a[1], b[1]) + EPS;
}
export function segmentsIntersect(a, b, c, d) {
  const abC = cross(a, b, c), abD = cross(a, b, d), cdA = cross(c, d, a), cdB = cross(c, d, b);
  if (((abC > EPS && abD < -EPS) || (abC < -EPS && abD > EPS)) &&
      ((cdA > EPS && cdB < -EPS) || (cdA < -EPS && cdB > EPS))) return true;
  return onSegment(a, b, c) || onSegment(a, b, d) || onSegment(c, d, a) || onSegment(c, d, b);
}
export function pointInPolygon(p, ring) {
  let inside = false;
  for (let i = 0, j = ring.length - 1; i < ring.length; j = i++) {
    const a = ring[j], b = ring[i];
    if (onSegment(a, b, p)) return true;
    if ((a[1] > p[1]) !== (b[1] > p[1]) && p[0] < (b[0] - a[0]) * (p[1] - a[1]) / (b[1] - a[1]) + a[0]) inside = !inside;
  }
  return inside;
}
export function polylineHitsPolygon(line, ring) {
  if (line.some(p => pointInPolygon(p, ring))) return true;
  for (let i = 1; i < line.length; i++) {
    for (let j = 0; j < ring.length; j++) {
      if (segmentsIntersect(line[i - 1], line[i], ring[j], ring[(j + 1) % ring.length])) return true;
    }
  }
  return false;
}

function validateArea(area) {
  if (!object(area) || !onlyKeys(area, new Set(["id", "name", "points"])) ||
      typeof area.id !== "string" || !/^[A-Za-z0-9_-]{1,64}$/.test(area.id) ||
      typeof area.name !== "string" || !area.name.trim() || area.name.length > 32 ||
      !Array.isArray(area.points) || area.points.length < 3 || area.points.length > 16 || !area.points.every(point)) invalid("避让区域需要名称及 3–16 个 WGS84 顶点");
  const ring = area.points.map(xy), n = ring.length;
  if (new Set(ring.map(p => p.join(","))).size !== n) invalid("避让区域不能包含重复顶点");
  for (let i = 0; i < n; i++) for (let j = i + 1; j < n; j++) {
    if (j === i + 1 || (i === 0 && j === n - 1)) continue;
    if (segmentsIntersect(ring[i], ring[(i + 1) % n], ring[j], ring[(j + 1) % n])) invalid("避让区域边界不能自相交");
  }
  // Translate before shoelace to avoid subtracting large absolute coordinates.
  const metresLat = 111_320, metresLon = metresLat * Math.cos(ring[0][1] * Math.PI / 180);
  const local = ring.map(p => [(p[0] - ring[0][0]) * metresLon, (p[1] - ring[0][1]) * metresLat]);
  const areaM2 = Math.abs(local.reduce((sum, p, i) => {
    const q = local[(i + 1) % n]; return sum + p[0] * q[1] - q[0] * p[1];
  }, 0)) / 2;
  if (areaM2 < 4 || areaM2 > 80_000_000) invalid("单个避让区域需在 4 平方米至 80 平方公里之间");
  // Stay below AMap's documented 81 km² per-polygon bound with a margin.
  return ring;
}

export function validateMotorcycleOptions(options) {
  if (!object(options) || !onlyKeys(options, KEYS) || options.region !== "nanjing" ||
      !REGISTRATIONS.has(options.registration) ||
      ![options.avoid_highways, options.avoid_tolls, options.avoid_ferries].every(v => typeof v === "boolean") ||
      !Array.isArray(options.avoid_areas) || options.avoid_areas.length > 4 ||
      !Array.isArray(options.waypoints) || options.waypoints.length > 6 || !options.waypoints.every(point)) invalid("南京骑行设置格式无效");
  options.avoid_areas.forEach(validateArea);
  if (new Set(options.avoid_areas.map(a => a.id)).size !== options.avoid_areas.length) invalid("避让区域标识不能重复");
  for (const p of options.waypoints) for (const area of options.avoid_areas) {
    if (pointInPolygon(xy(p), area.points.map(xy))) invalid("途经点不能位于自定义避让区域内");
  }
  return options;
}

export function validateMotorcycleEndpoints(request) {
  const options = validateMotorcycleOptions(request.motorcycle_options);
  if (MOTU_NANJING.applies_to.includes(options.registration)) {
    const targets = [["起点", request.origin], ["终点", request.destination],
      ...options.waypoints.map((point, index) => [`第 ${index + 1} 个途经点`, point])];
    for (const [name, point] of targets) if (pointInPolygon(xy(wgs84ToGcj02(point)), MOTU_NANJING.polygon)) {
      throw new MotorcyclePolicyError("MOTORCYCLE_AREA_CONFLICT",
        `${name}位于南京禁外摩区域（摩途官网图层），与当前登记类型冲突，请调整位置。`);
    }
  }
  for (const area of options.avoid_areas) {
    if ([request.origin, request.destination].some(p => pointInPolygon(xy(p), area.points.map(xy)))) {
      throw new MotorcyclePolicyError("AVOIDANCE_ENDPOINT_CONFLICT", "起点或终点位于自定义避让区域内，请调整区域或目的地");
    }
  }
}

export function motorcycleStrategy(options) {
  if (options.avoid_highways) return options.avoid_tolls ? 43 : 40;
  return options.avoid_tolls ? 41 : 33;
}

export function applyMotorcycleParameters(url, options) {
  validateMotorcycleOptions(options);
  url.searchParams.set("strategy", String(motorcycleStrategy(options)));
  url.searchParams.set("ferry", options.avoid_ferries ? "1" : "0");
  const format = p => {
    const gcj = wgs84ToGcj02(p);
    return `${gcj.longitude_deg.toFixed(6)},${gcj.latitude_deg.toFixed(6)}`;
  };
  if (options.waypoints.length) url.searchParams.set("waypoints", options.waypoints.map(format).join(";"));
  // Motu values already use the GCJ-02 renderer's coordinates. Only manual
  // WGS84 areas are transformed; never transform the source a second time.
  const regions = MOTU_NANJING.applies_to.includes(options.registration)
    ? MOTU_NANJING.avoid_polygons.map(ring => ring.map(p => `${p[0].toFixed(6)},${p[1].toFixed(6)}`).join(";")) : [];
  regions.push(...options.avoid_areas.map(area => area.points.map(format).join(";")));
  if (regions.length) url.searchParams.set("avoidpolygons", regions.join("|"));
  // No cartype=11: the v5 car Web API documents only car types 0/1/2.
}

export function motorcycleRestrictionAreas(options) {
  return [{
    id: MOTU_NANJING.id,
    name: MOTU_NANJING.name,
    source_url: MOTU_NANJING.source_url,
    data_version: MOTU_NANJING.data_version,
    coordinate_system: "GCJ-02",
    applies_to_registration: MOTU_NANJING.applies_to.includes(options.registration),
    polygon: MOTU_NANJING.polygon.map(([longitude_deg, latitude_deg]) => ({ longitude_deg, latitude_deg })),
  }];
}

function roadMatches(name, aliases) {
  const cleaned = String(name ?? "").replace(/[\s（）()]/g, "");
  return aliases.some(alias => cleaned === alias || cleaned === `${alias}主桥` || cleaned === `${alias}公路桥`);
}

function inNanjing(step) {
  const cities = Array.isArray(step.cities) ? step.cities : [];
  if (cities.some(city => String(city.adcode ?? "").startsWith("3201") || city.city === "南京市")) return true;
  // A coarse envelope is ONLY for disambiguating road names in this city.
  // It is never a prohibited-area polygon or evidence of permission to ride.
  return String(step.polyline ?? "").split(";").some(pair => {
    const [lon, lat] = pair.split(",").map(Number);
    return lon >= 118.3 && lon <= 119.4 && lat >= 31.15 && lat <= 32.8;
  });
}

export function assessMotorcycleRoute(route, rawPath, request, checkedAtMs = Date.now()) {
  const options = request.motorcycle_options;
  const violations = [];
  const steps = Array.isArray(rawPath.steps) ? rawPath.steps : [];
  const roadNames = steps.filter(inNanjing).map(step => step.road_name ?? step.road ?? "");
  // Use the complete upstream road names before the transformer truncates text.
  for (const aliases of CROSSINGS) if (roadNames.some(name => roadMatches(name, aliases))) violations.push(aliases[0]);
  if (options.registration !== "nanjing_urban" && roadNames.some(name => roadMatches(name, OLD_BRIDGE))) violations.push("南京长江大桥（当前登记类型未获通行确认）");
  // A surviving step fragment is not a complete route. Match the transformer's
  // complete-path fallback when any step geometry is absent.
  const parseLine = value => String(value ?? "").split(";")
    .filter(pair => pair.trim()).map(pair => pair.split(",").map(Number));
  const stepLines = steps.map(step => parseLine(step.polyline));
  const fullLine = stepLines.length && stepLines.every(line => line.length >= 2)
    ? stepLines.flat() : parseLine(rawPath.polyline);
  const line = fullLine.length >= 2 ? fullLine : route.polyline.map(xy);
  if (MOTU_NANJING.applies_to.includes(options.registration) && polylineHitsPolygon(line, MOTU_NANJING.polygon)) {
    violations.push("南京禁外摩区域（摩途官网图层）");
  }
  for (const area of options.avoid_areas) {
    const ring = area.points.map(wgs84ToGcj02).map(xy);
    if (polylineHitsPolygon(line, ring)) violations.push(`自定义避让区：${area.name}`);
  }
  const warnings = [
    "已按摩途官网南京图层检查完整路线，并检查已收录过江通道及自定义避让区；规则覆盖仍不完整。",
    `禁行区数据为摩途公开图层快照（${MOTU_NANJING.retrieved_on}获取），不代表官方实时通行许可。`,
    "城市高架、临时及分时管制未完整收录；限行电子眼数据未加载，不能据此判断没有抓拍点。",
    "道路名称缺失、别名变化或同名道路可能影响识别；请核对实际路线与现场标志。",
    "偏航重算后仍需核对新路线；当前工具不能判定整条路线的摩托车通行资格。",
  ];
  if (options.registration === "nanjing_suburban" || options.registration === "non_nanjing") warnings.unshift(
    `当前为${options.registration === "nanjing_suburban" ? "南京郊区登记号牌" : "外地号牌"}；已避让并复查摩途官网南京禁外摩区域，请核对实际路线与现场标志。`
  );
  if (options.registration === "nanjing_urban") warnings.unshift("当前为南京主城区登记号牌，未套用禁外摩区域避让；已收录桥隧及自定义区域仍会检查。");
  if (options.registration === "unknown") warnings.unshift("尚未确定车辆登记类型，不能据苏A号牌推断主城区通行资格。");
  return {
    violations,
    assessment: {
      policy_version: NANJING_POLICY.version,
      verified_on: NANJING_POLICY.verified_on,
      coverage: "partial",
      status: "review_required",
      checked_at_ms: Math.max(0, Math.trunc(checkedAtMs)),
      applied_options: options,
      warnings,
      sources: NANJING_POLICY.sources,
      restriction_areas: motorcycleRestrictionAreas(options),
      camera_data_status: "unavailable",
    },
  };
}
