import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {runInNewContext} from 'node:vm';
import test from 'node:test';
import {wgs84ToGcj02} from '../src/coordinates.js';
import {sdkAvoidanceParameters, sdkDetourWaypoints} from '../src/ios-sdk-routing.js';
const script=readFileSync(new URL('../../platforms/ios/App/Adapters/Navigation/SDKMotorcyclePolicy.generated.js',import.meta.url),'utf8');
const ctx={};runInNewContext(script,ctx);
const p=(longitude_deg,latitude_deg)=>({coordinate_system:'WGS84',longitude_deg,latitude_deg});
const request=()=>({origin:p(118.7,31.9),destination:p(118.9,31.9),motorcycle_options:{region:'nanjing',registration:'nanjing_suburban',avoid_highways:true,avoid_elevated_roads:true,avoid_tolls:false,avoid_ferries:true,avoid_areas:[],waypoints:[]}});
const route=()=>({schema_version:1,route_id:'sdk-route',provider:'amap_ios_sdk',coordinate_system:'GCJ-02',generated_at_ms:1,total_distance_m:18000,total_duration_s:1500,polyline:[p(118.7,31.9),p(118.9,31.9)].map(wgs84ToGcj02),maneuvers:[],traffic:[],sdk_steps:[],sdk_has_highway:false,sdk_has_ferry:false,sdk_has_elevated:false,sdk_elevated_checked:true});
const check=(r=request(),line=route(),seed=false)=>JSON.parse(ctx.checkSDKRoute(JSON.stringify(r),line===null?'':JSON.stringify(line),seed));
test('SDK route is usable without a restriction assessment or review requirement',()=>{
 const line=route();line.motorcycle_assessment={status:'review_required'};
 const value=check(request(),line);assert.equal(value.ok,true);
 assert.equal(value.route.motorcycle_assessment,undefined);
 assert.equal(value.route.polyline.length,2);
 assert.equal(line.motorcycle_assessment.status,'review_required');
 assert.doesNotMatch(script,/MOTU_NANJING|NANJING_POLICY|assessMotorcycleRoute|validateMotorcycleEndpoints|CROSSINGS/);
});
test('missing settings fail closed before SDK route planning',()=>{
 const r=request();delete r.motorcycle_options;assert.equal(check(r,null).ok,false);
});
test('all registrations can plan with formerly restricted start, destination and waypoint',()=>{
 for(const registration of ['nanjing_suburban','nanjing_urban','non_nanjing','unknown'])
 for(const target of ['origin','destination','waypoint']){
  const r=request(),inside=p(118.784,32.041);r.motorcycle_options.registration=registration;
  if(target==='waypoint')r.motorcycle_options.waypoints=[inside];else r[target]=inside;
  const before=JSON.stringify(r);
  assert.equal(check(r,null).ok,true);
  const parameters=sdkAvoidanceParameters(r);
  assert.deepEqual(parameters.avoid_polygons,[]);
  assert.deepEqual(parameters.waypoints,r.motorcycle_options.waypoints.map(wgs84ToGcj02));
  assert.equal(JSON.stringify(r),before);
 }
});
test('formerly restricted area crossing is accepted on preview and reroute',()=>{
 const r=request(),line=route();line.polyline=[r.origin,p(118.784,32.041),r.destination].map(wgs84ToGcj02);
 for(const reroute of [false,true]){r.is_reroute=reroute;assert.equal(check(r,line).ok,true);}
});
test('SDK sparse geometry crossing an avoided area is blocked',()=>{
 const r=request();r.motorcycle_options.avoid_areas=[{id:'zone',name:'test area',points:[p(118.75,31.89),p(118.76,31.89),p(118.76,31.91),p(118.75,31.91)]}];
 const value=check(r);assert.equal(value.ok,false);assert.match(value.message,/test area/);
});
test('bridge and tunnel names do not block rerouting',()=>{
 const r=request();r.is_reroute=true;const line=route();
 line.sdk_steps=[{road_name:'建宁路长江隧道',polyline:'118.73,32.1;118.74,32.11',link_type:2}];assert.equal(check(r,line).ok,true);
});
test('SDK highway and ferry flags obey rider options',()=>{
 for(const [flag,option] of [['sdk_has_highway','avoid_highways'],['sdk_has_ferry','avoid_ferries']]){
  const r=request(),line=route();line[flag]=true;assert.equal(check(r,line).ok,false);
  r.motorcycle_options[option]=false;assert.equal(check(r,line).ok,true);
 }
});
test('elevated flag and unnamed typed links are blocked on preview and reroute',()=>{
 for(const reroute of [false,true])for(const method of ['flag','link']){
  const r=request(),line=route();r.is_reroute=reroute;
  if(method==='flag')line.sdk_has_elevated=true;
  else line.sdk_steps=[{road_name:'',link_type:4}];
  const value=check(r,line);assert.equal(value.ok,false);assert.match(value.message,/高架/);
  r.motorcycle_options.avoid_elevated_roads=false;assert.equal(check(r,line).ok,true);
 }
});
test('unknown link coverage fails closed when avoiding elevated roads',()=>{
 for(const missing of [true,false]){
  const line=route();if(missing)delete line.sdk_elevated_checked;else line.sdk_elevated_checked=false;
  assert.equal(check(request(),line).code,'ROUTE_ROAD_TYPE_UNAVAILABLE');
  const r=request();r.motorcycle_options.avoid_elevated_roads=false;assert.equal(check(r,line).ok,true);
 }
});
test('ordinary bridges and surface main streets are not inferred to be elevated',()=>{
 const line=route();line.sdk_steps=[{road_name:'地面主干道',road_class:6,link_type:0},{road_name:'普通桥梁',road_class:6,link_type:3}];
 assert.equal(check(request(),line).ok,true);
});
test('Search corridor may lack link types but can never become a navigation route',()=>{
 const r=request(),line=route();delete line.sdk_elevated_checked;line.provider='amap_ios_search_sdk';
 assert.equal(check(r,line,true).ok,true);assert.equal(check(r,line).ok,false);
 line.polyline=[r.origin,p(118.784,32.041),r.destination].map(wgs84ToGcj02);
 assert.equal(check(r,line,true).ok,true);
 line.provider='amap_ios_sdk';assert.equal(check(r,line,true).ok,false);
});
test('turning off elevated avoidance retains highway and ferry preferences',()=>{
 const r=request();r.motorcycle_options.avoid_elevated_roads=false;
 const line=route();line.sdk_has_elevated=true;line.sdk_has_highway=true;
 assert.equal(check(r,line).ok,false);
 line.sdk_has_highway=false;line.sdk_has_ferry=true;assert.equal(check(r,line).ok,false);
 line.sdk_has_ferry=false;line.polyline=[r.origin,p(118.784,32.041),r.destination].map(wgs84ToGcj02);
 assert.equal(check(r,line).ok,true);
});
test('missing or nonfinite route geometry fails closed',()=>{
 for(const points of [[],[p(118,32)],[p(118,32),p(190,32)],[p(118,32),p(null,32)]]){
  const line=route();line.polyline=points;assert.equal(check(request(),line).ok,false);
 }
});
test('SDK policy uses all vertices beyond BLE window',()=>{
 const r=request();r.motorcycle_options.avoid_areas=[{id:'zone',name:'late area',points:[p(118.75,31.89),p(118.76,31.89),p(118.76,31.91),p(118.75,31.91)]}];
 const line=route();line.polyline=Array.from({length:150},(_,i)=>wgs84ToGcj02(p(118.70+i*0.0001,31.9)));
 line.polyline.push(wgs84ToGcj02(p(118.8,31.9)));assert.equal(check(r,line).ok,false);
});
test('Search SDK receives only converted user areas and retains route preferences',()=>{
 const r=request();r.destination_poi_id='B123';r.motorcycle_options.avoid_tolls=true;
 const manual=[p(118.6,31.8),p(118.61,31.8),p(118.61,31.81)];
 r.motorcycle_options.avoid_areas=Array.from({length:4},(_,i)=>({id:'zone'+i,name:'zone'+i,points:manual}));
 const parameters=sdkAvoidanceParameters(r);assert.equal(parameters.avoid_polygons.length,4);
 assert.deepEqual(parameters.avoid_polygons.at(-1),manual.map(wgs84ToGcj02));
 assert.deepEqual(parameters.origin,wgs84ToGcj02(r.origin));
 assert.equal(parameters.search_strategy,43);assert.equal(parameters.strategy,18);assert.equal(parameters.ferry,1);assert.equal(parameters.poi_id,'B123');
 const bundled=JSON.parse(ctx.prepareSDKAvoidance(JSON.stringify(r),'',8));
 assert.equal(bundled.ok,true);assert.deepEqual(bundled.parameters,parameters);
 r.motorcycle_options.registration='nanjing_urban';assert.equal(sdkAvoidanceParameters(r).avoid_polygons.length,4);
});
test('detour hints keep ordered manual waypoints, road vertices and the SDK 16-point limit',()=>{
 const r=request();r.motorcycle_options.waypoints=[p(118.75,31.875),p(118.85,31.875)];
 const original=JSON.stringify(r);
 const line=Array.from({length:81},(_,i)=>wgs84ToGcj02(p(118.7+i*0.0025,31.85+Math.abs(i-40)*0.00125)));
 for(const limit of [8,16]){
  const hints=sdkDetourWaypoints(r,line,limit);assert.equal(hints.length,limit);
  const manual=r.motorcycle_options.waypoints.map(wgs84ToGcj02);
  const positions=manual.map(point=>hints.findIndex(hint=>JSON.stringify(hint)===JSON.stringify(point)));
  assert.ok(positions[0]>=0 && positions[1]>positions[0]);
  assert.ok(hints.some(hint=>JSON.stringify(hint)===JSON.stringify(line[40])),'major detour bend survives');
  assert.ok(hints.every(hint=>[...manual,...line].some(point=>JSON.stringify(hint)===JSON.stringify(point))));
 }
 assert.equal(JSON.stringify(r),original);
});
test('invalid detour geometry and oversized waypoint budgets are rejected',()=>{
 for(const [line,limit] of [[[],8],[route().polyline,17],[[p(null,32),p(118,32)],8]]){
  assert.throws(()=>sdkDetourWaypoints(request(),line,limit),{code:'ROUTE_PREFERENCE_CONFLICT'});
 }
});


test('custom area crossings and endpoints are still checked even with a restriction-like name',()=>{
 const r=request();r.motorcycle_options.avoid_areas=[{id:'saved',name:'南京禁外摩区域',points:[p(118.75,31.89),p(118.76,31.89),p(118.76,31.91),p(118.75,31.91)]}];
 assert.equal(check(r,route()).ok,false);
 r.origin=p(118.755,31.9);
 assert.equal(check(r,null).code,'AVOIDANCE_ENDPOINT_CONFLICT');
 assert.throws(()=>sdkAvoidanceParameters(r),{code:'AVOIDANCE_ENDPOINT_CONFLICT'});
 r.origin=p(118.7,31.9);r.motorcycle_options.waypoints=[p(118.755,31.9)];
 assert.equal(check(r,null).code,'INVALID_REQUEST');
});

test('Search guidance preserves waypoints inside formerly restricted areas',()=>{
 const r=request();r.motorcycle_options.waypoints=[p(118.784,32.041)];
 const line=route();line.polyline=[r.origin,r.motorcycle_options.waypoints[0],r.destination].map(wgs84ToGcj02);
 line.provider='amap_ios_search_sdk';delete line.sdk_elevated_checked;
 assert.equal(check(r,line,true).ok,true);
 assert.equal(check(r,line).ok,false);
 const hints=sdkDetourWaypoints(r,line.polyline,8);
 assert.ok(hints.some(h=>JSON.stringify(h)===JSON.stringify(wgs84ToGcj02(r.motorcycle_options.waypoints[0]))));
});
