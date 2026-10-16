// Pure request/geometry helpers shared by JavaScriptCore and regression tests.
import { wgs84ToGcj02 } from './coordinates.js';
import { validateMotorcycleOptions, motorcycleStrategy, MotorcyclePolicyError, pointInPolygon, polylineHitsPolygon } from './routing-preferences.js';

export function validateSDKRequest(request) {
  const options = validateMotorcycleOptions(request.motorcycle_options);
  for (const area of options.avoid_areas) {
    if ([request.origin, request.destination].some(p =>
        pointInPolygon([p.longitude_deg, p.latitude_deg], area.points.map(p => [p.longitude_deg, p.latitude_deg])))) {
      throw new MotorcyclePolicyError('AVOIDANCE_ENDPOINT_CONFLICT', '起点或终点位于自定义避让区域内，请调整区域或目的地');
    }
  }
  return options;
}

export function sdkAvoidanceParameters(request) {
  const options = validateSDKRequest(request);
  const polygons = options.avoid_areas.map(area => area.points.map(wgs84ToGcj02));
  return {origin: wgs84ToGcj02(request.origin), destination: wgs84ToGcj02(request.destination),
    poi_id: request.destination_poi_id || '', waypoints: options.waypoints.map(wgs84ToGcj02),
    avoid_polygons: polygons, ferry: options.avoid_ferries ? 1 : 0,
    search_strategy: motorcycleStrategy(options),
    strategy: options.avoid_highways ? (options.avoid_tolls ? 18 : 15) : (options.avoid_tolls ? 17 : 12)};
}

// Check only the rider's explicit preferences. Regional restriction polygons,
// vehicle registration and bridge/tunnel name lists do not filter SDK routes.
export function sdkCheckedRoute(request, route, searchSeed = false) {
  const options = validateSDKRequest(request);
  if (route.provider !== (searchSeed ? 'amap_ios_search_sdk' : 'amap_ios_sdk'))
    throw new MotorcyclePolicyError('ROUTE_PREFERENCE_CONFLICT', '路线来源不匹配，请重新规划。');
  if (!Array.isArray(route.polyline) || route.polyline.length < 2 || !route.polyline.every(p =>
      Number.isFinite(p.longitude_deg) && Number.isFinite(p.latitude_deg) &&
      Math.abs(p.longitude_deg) <= 180 && Math.abs(p.latitude_deg) <= 85))
    throw new MotorcyclePolicyError('ROUTE_PREFERENCE_CONFLICT', 'SDK 路线坐标无效，请重新规划。');
  const line = route.polyline.map(p => [p.longitude_deg, p.latitude_deg]);
  const violations = options.avoid_areas.filter(area =>
    polylineHitsPolygon(line, area.points.map(wgs84ToGcj02).map(p => [p.longitude_deg, p.latitude_deg])))
    .map(area => '自定义避让区：' + area.name);
  if (options.avoid_highways && route.sdk_has_highway) violations.push('高速公路');
  if (options.avoid_ferries && route.sdk_has_ferry) violations.push('轮渡');
  // Search paths are guidance only. Actual navigation must be checked using
  // complete typed links when the rider explicitly enables elevated avoidance.
  if (options.avoid_elevated_roads && !searchSeed) {
    if (route.sdk_has_elevated || (route.sdk_steps || []).some(step => step.link_type === 4)) {
      violations.push('高架道路（已开启避开高架）');
    } else if (route.sdk_elevated_checked !== true) {
      throw new MotorcyclePolicyError('ROUTE_ROAD_TYPE_UNAVAILABLE', '路线缺少完整的道路类型，无法确认避开高架，请重新规划。');
    }
  }
  if (violations.length) throw new MotorcyclePolicyError('ROUTE_PREFERENCE_CONFLICT',
    '路线经过：' + violations.join('、') + '。请调整路线偏好、途经点或目的地。');
  const checked = {...route};
  delete checked.motorcycle_assessment;
  return checked;
}

// Select on-road guidance points from a checked Search SDK path. Never fabricate
// a geographic detour across water or private land. Navi results are checked
// again: these hints do not guarantee that its road routing follows the seed.
export function sdkDetourWaypoints(request, line, limit) {
  const manual = sdkAvoidanceParameters(request).waypoints;
  if (![8, 16].includes(limit) || !Array.isArray(line) || line.length < 2 ||
      !line.every(p => Number.isFinite(p.longitude_deg) && Number.isFinite(p.latitude_deg) &&
        Math.abs(p.longitude_deg) <= 180 && Math.abs(p.latitude_deg) <= 85)) {
    throw new MotorcyclePolicyError('ROUTE_PREFERENCE_CONFLICT', '绕行路线缺少有效的完整道路坐标。');
  }
  const metresLon = 111320 * Math.cos(line[0].latitude_deg * Math.PI / 180);
  const local = p => [p.longitude_deg * metresLon, p.latitude_deg * 111320];
  const points = line.map(local);
  const dist2 = (a, b) => (a[0]-b[0])**2 + (a[1]-b[1])**2;
  const arc = [0];
  for (let i=1; i<points.length; i++) arc.push(arc[i-1] + Math.sqrt(dist2(points[i-1],points[i])));
  let previous = 0;
  const anchors = manual.map((point, order) => {
    let index = previous, best = Infinity;
    for (let i=previous; i<points.length; i++) {
      const d = dist2(local(point),points[i]);
      if (d<best) { best=d; index=i; }
    }
    previous=index;
    return {index, order, point};
  });
  const selected = new Set([0,line.length-1,...anchors.map(a=>a.index)]);
  while (anchors.length < limit) {
    const indices = [...selected].sort((a,b)=>a-b);
    let bestIndex=-1, bestScore=-1;
    for (let j=1; j<indices.length; j++) {
      const start=indices[j-1], end=indices[j], a=points[start], b=points[end];
      const span2=dist2(a,b);
      for (let i=start+1; i<end; i++) {
        if (Math.min(arc[i]-arc[start],arc[end]-arc[i]) < 30) continue;
        const p=points[i], t=span2 ? Math.max(0,Math.min(1,((p[0]-a[0])*(b[0]-a[0])+(p[1]-a[1])*(b[1]-a[1]))/span2)) : 0;
        const deviation=Math.sqrt(dist2(p,[a[0]+t*(b[0]-a[0]),a[1]+t*(b[1]-a[1])]));
        // Preserve major bends first; also split long straight spans.
        const score=deviation + Math.min(arc[i]-arc[start],arc[end]-arc[i])*0.02;
        if (score>bestScore) { bestScore=score; bestIndex=i; }
      }
    }
    if (bestIndex<0) break;
    selected.add(bestIndex);
    anchors.push({index:bestIndex,order:anchors.length,point:line[bestIndex]});
  }
  return anchors.sort((a,b)=>a.index-b.index || a.order-b.order).map(a=>a.point);
}
