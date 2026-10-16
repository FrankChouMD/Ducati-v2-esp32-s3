// Shared preference validation and geometry. No regional traffic restrictions.
const REGISTRATIONS = new Set(["nanjing_suburban", "nanjing_urban", "non_nanjing", "unknown"]);
const KEYS = new Set(["region", "registration", "avoid_highways", "avoid_elevated_roads", "avoid_tolls", "avoid_ferries", "avoid_areas", "waypoints"]);
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
      (options.avoid_elevated_roads !== undefined && typeof options.avoid_elevated_roads !== "boolean") ||
      !Array.isArray(options.avoid_areas) || options.avoid_areas.length > 4 ||
      !Array.isArray(options.waypoints) || options.waypoints.length > 6 || !options.waypoints.every(point)) invalid("骑行设置格式无效");
  options.avoid_areas.forEach(validateArea);
  if (new Set(options.avoid_areas.map(a => a.id)).size !== options.avoid_areas.length) invalid("避让区域标识不能重复");
  for (const p of options.waypoints) for (const area of options.avoid_areas) {
    if (pointInPolygon(xy(p), area.points.map(xy))) invalid("途经点不能位于自定义避让区域内");
  }
  return options;
}

export function motorcycleStrategy(options) {
  if (options.avoid_highways) return options.avoid_tolls ? 43 : 40;
  return options.avoid_tolls ? 41 : 33;
}

