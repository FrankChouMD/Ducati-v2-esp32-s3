#import "AMapSDKBridge.h"
#import "MotoNavSnapshotInternal.h"
#import <AMapFoundationKit/AMapFoundationKit.h>
#import <AMapSearchKit/AMapSearchKit.h>
#import <AMapNaviKit/AMapNaviKit.h>
#import <CoreLocation/CoreLocation.h>

static NSError *SDKError(NSString *message) {
    return [NSError errorWithDomain:@"MotoGPS.AMapSDK" code:1 userInfo:@{NSLocalizedDescriptionKey:message}];
}
static NSDictionary *SDKPoint(AMapNaviPoint *p) {
    return @{@"longitude_deg":@(p.longitude), @"latitude_deg":@(p.latitude)};
}
static NSString *Maneuver(NSInteger icon) {
    switch (icon) {
        case 2:return @"left"; case 3:return @"right";
        case 4:return @"slight_left"; case 5:return @"slight_right";
        case 6:return @"sharp_left"; case 7:return @"sharp_right";
        case 8:return @"u_turn_left"; case 19:return @"u_turn_right";
        case 11:return @"roundabout"; case 12:return @"exit";
        case 15:return @"arrive"; case 9:return @"continue";
        default:return @"unknown";
    }
}
static NSString *Traffic(NSInteger status) {
    return status >= 1 && status <= 4 ? @[@"unknown",@"free_flow",@"slow",@"congested",@"severe"][status] : @"unknown";
}
static AMapNaviPOIInfo *POI(NSDictionary *p) {
    AMapNaviPOIInfo *poi = [AMapNaviPOIInfo new];
    poi.locPoint = [AMapNaviPoint locationWithLatitude:[p[@"latitude_deg"] doubleValue] longitude:[p[@"longitude_deg"] doubleValue]];
    return poi;
}
static AMapGeoPoint *SearchPoint(NSDictionary *p) {
    return [AMapGeoPoint locationWithLatitude:[p[@"latitude_deg"] doubleValue] longitude:[p[@"longitude_deg"] doubleValue]];
}
static NSDictionary *SearchRoute(AMapPath *path) {
    NSMutableArray *line=[NSMutableArray array], *steps=[NSMutableArray array];
    for (AMapStep *step in path.steps) {
        NSArray *pairs=[step.polyline componentsSeparatedByString:@";"];
        if(pairs.count<2)return nil; // Never use a surviving fragment as a complete seed.
        for (NSString *pair in pairs) {
            NSArray *parts=[pair componentsSeparatedByString:@","];
            if(parts.count!=2)return nil;
            double lon=0,lat=0;
            NSScanner *x=[NSScanner scannerWithString:parts[0]], *y=[NSScanner scannerWithString:parts[1]];
            if(![x scanDouble:&lon] || !x.isAtEnd || ![y scanDouble:&lat] || !y.isAtEnd ||
               !isfinite(lon) || !isfinite(lat) || fabs(lon)>180 || fabs(lat)>85)return nil;
            NSDictionary *point=@{@"longitude_deg":@(lon),@"latitude_deg":@(lat)};
            if(![line.lastObject isEqual:point])[line addObject:point];
        }
        [steps addObject:@{@"road_name":step.road ?: @"",@"polyline":step.polyline}];
    }
    if(line.count<2)return nil;
    return @{@"schema_version":@1,@"route_id":NSUUID.UUID.UUIDString,@"provider":@"amap_ios_search_sdk",
        @"coordinate_system":@"GCJ-02",@"generated_at_ms":@((uint64_t)(NSDate.date.timeIntervalSince1970*1000)),
        @"total_distance_m":@(path.distance),@"total_duration_s":@(path.duration),@"polyline":line,
        @"maneuvers":@[],@"traffic":@[],@"sdk_steps":steps};
}
static NSDictionary *Route(AMapNaviRoute *r, NSString *identifier) {
    NSMutableArray *line=[NSMutableArray array], *steps=[NSMutableArray array], *maneuvers=[NSMutableArray array], *traffic=[NSMutableArray array];
    for (AMapNaviPoint *p in r.routeCoordinates) [line addObject:SDKPoint(p)];
    double offset=0;
    BOOL highway=NO, ferry=NO, elevated=NO, allLinkTypesKnown=r.routeSegments.count>0;
    NSUInteger linkCount=0;
    for (AMapNaviSegment *segment in r.routeSegments) {
        if(segment.links.count==0)allLinkTypesKnown=NO;
        for (AMapNaviLink *link in segment.links) {
            NSMutableArray *linkLine=[NSMutableArray array];
            for (AMapNaviPoint *p in link.coordinates) [linkLine addObject:[NSString stringWithFormat:@"%.7f,%.7f",p.longitude,p.latitude]];
            [steps addObject:@{@"road_name":link.roadName ?: @"", @"polyline":[linkLine componentsJoinedByString:@";"], @"link_type":@(link.linkType)}];
            highway |= link.roadClass == AMapNaviRoadClassHighWay;
            elevated |= link.linkType == AMapNaviLinkTypeElevatedRd;
            allLinkTypesKnown &= link.linkType >= AMapNaviLinkTypeNormalRoad && link.linkType <= AMapNaviLinkTypeElevatedRd;
            linkCount++;
            [traffic addObject:@{@"start_offset_m":@(offset),@"end_offset_m":@(offset+link.length),@"level":Traffic(link.trafficStatus)}];
            offset+=link.length;
        }
        ferry |= segment.iconType == AMapNaviIconTypeFerryboat;
        [maneuvers addObject:@{@"id":@(maneuvers.count+1),@"type":Maneuver(segment.iconType),@"route_offset_m":@(offset),
            @"road_name":segment.links.lastObject.roadName ?: @"",@"instruction":@"",@"roundabout_exit":@0}];
    }
    // The complete SDK geometry is always retained for the local policy checker.
    return @{@"schema_version":@1,@"route_id":identifier,@"provider":@"amap_ios_sdk",@"coordinate_system":@"GCJ-02",
        @"generated_at_ms":@((uint64_t)(NSDate.date.timeIntervalSince1970*1000)),
        @"total_distance_m":@(r.routeLength),@"total_duration_s":@(r.routeTime),@"polyline":line,
        @"maneuvers":maneuvers,@"traffic":traffic,@"sdk_steps":steps,@"sdk_has_highway":@(highway),@"sdk_has_ferry":@(ferry),
        @"sdk_has_elevated":@(elevated),@"sdk_elevated_checked":@(linkCount>0 && allLinkTypesKnown)};
}

@interface AMapSDKBridge () <AMapSearchDelegate, AMapNaviDriveManagerDelegate, AMapNaviDriveDataRepresentable>
@property(nonatomic,strong) AMapSearchAPI *searchAPI;
@property(nonatomic,strong) NSMutableDictionary *searches;
@property(nonatomic,strong) NSMutableDictionary<NSString *,AMapNaviRouteGroup *> *groups;
@property(nonatomic,strong) NSMutableDictionary<NSString *,NSNumber *> *routeIDs;
@property(nonatomic,strong) AMapNaviDriveManager *manager;
@property(nonatomic,strong) AMapNaviRoute *route;
@property(nonatomic,strong) AMapNaviInfo *info;
@property(nonatomic,strong) AMapNaviLocation *location;
@property(nonatomic,strong) MotoNavCoreSnapshot *lastRouteView;
@property(nonatomic,copy) NSString *routeID;
@property(nonatomic,copy) NSString *page;
@property(nonatomic,copy) NSString *state;
@property(nonatomic) BOOL active;
@property(nonatomic) BOOL networkAvailable;
@property(nonatomic) BOOL awaitingRoute;
@property(nonatomic) NSUInteger calculationGeneration;
@property(nonatomic) uint32_t generation;
@property(nonatomic,strong) NSTimer *timer;
@end

@implementation AMapSDKBridge
+ (void)configureKey:(NSString *)key {
    // Invoked only after the app has presented the privacy notice and the rider agrees.
    [AMapServices sharedServices].apiKey=key;
    [AMapSearchAPI updatePrivacyShow:AMapPrivacyShowStatusDidShow privacyInfo:AMapPrivacyInfoStatusDidContain];
    [AMapSearchAPI updatePrivacyAgree:AMapPrivacyAgreeStatusDidAgree];
    [[AMapNaviManagerConfig sharedConfig] updatePrivacyShow:AMapPrivacyShowStatusDidShow privacyInfo:AMapPrivacyInfoStatusDidContain];
    [[AMapNaviManagerConfig sharedConfig] updatePrivacyAgree:AMapPrivacyAgreeStatusDidAgree];
    [MAMapView updatePrivacyShow:AMapPrivacyShowStatusDidShow privacyInfo:AMapPrivacyInfoStatusDidContain];
    [MAMapView updatePrivacyAgree:AMapPrivacyAgreeStatusDidAgree];
}
- (instancetype)init {
    if ((self=[super init])) {
        _searches=[NSMutableDictionary dictionary]; _groups=[NSMutableDictionary dictionary]; _routeIDs=[NSMutableDictionary dictionary];
        _networkAvailable=YES;_page=@"navigation"; _state=@"idle"; _routeID=@"";
    }
    return self;
}
- (void)prepare {
    if (!self.manager) {
        self.manager=[AMapNaviDriveManager sharedInstance];
        self.manager.delegate=self; [self.manager addDataRepresentative:self];
        self.manager.allowsBackgroundLocationUpdates=YES;
        self.manager.pausesLocationUpdatesAutomatically=NO;
        self.manager.isUseInternalTTS=YES;
        [self.manager setMultipleRouteNaviMode:NO];
        self.searchAPI=[AMapSearchAPI new]; self.searchAPI.delegate=self;
    }
}
- (void)search:(NSString *)query longitude:(double)longitude latitude:(double)latitude completion:(void (^)(NSArray<NSDictionary<NSString *, id> *> *,NSError *))completion {
    [self prepare];
    AMapPOIKeywordsSearchRequest *request=[AMapPOIKeywordsSearchRequest new];
    request.keywords=query; request.offset=20; request.page=1;
    if (isfinite(longitude) && isfinite(latitude)) request.location=[AMapGeoPoint locationWithLatitude:latitude longitude:longitude];
    NSValue *token=[NSValue valueWithNonretainedObject:request]; self.searches[token]=[completion copy];
    [self.searchAPI AMapPOIKeywordsSearch:request];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,15*NSEC_PER_SEC),dispatch_get_main_queue(),^{
        (void)request; // Retain identity until the timeout cannot hit a later request at the same address.
        void (^pending)(NSArray *,NSError *)=self.searches[token];
        [self.searches removeObjectForKey:token]; if(pending) pending(nil,SDKError(@"高德搜索超时，请检查网络或 iOS Key。"));
    });
}
- (void)onPOISearchDone:(AMapPOISearchBaseRequest *)request response:(AMapPOISearchResponse *)response {
    NSValue *token=[NSValue valueWithNonretainedObject:request];
    void (^completion)(NSArray *,NSError *)=self.searches[token]; [self.searches removeObjectForKey:token]; if(!completion)return;
    NSMutableArray *places=[NSMutableArray array];
    for(AMapPOI *p in response.pois) if(p.location) [places addObject:@{@"id":p.uid ?: @"",@"name":p.name ?: @"",@"address":p.address ?: @"",@"city":p.city ?: @"",@"district":p.district ?: @"",@"longitude_deg":@(p.location.longitude),@"latitude_deg":@(p.location.latitude)}];
    completion(places,nil);
}
- (void)AMapSearchRequest:(id)request didFailWithError:(NSError *)error {
    NSValue *token=[NSValue valueWithNonretainedObject:request];
    void (^completion)(NSArray *,NSError *)=self.searches[token]; [self.searches removeObjectForKey:token]; if(completion)completion(nil,error);
}
- (AMapDrivingCalRouteSearchRequest *)makeAvoidanceRequest:(NSDictionary *)parameters {
    AMapDrivingCalRouteSearchRequest *request=[AMapDrivingCalRouteSearchRequest new];
    request.origin=SearchPoint(parameters[@"origin"]);request.destination=SearchPoint(parameters[@"destination"]);
    request.destinationId=parameters[@"poi_id"];
    request.avoidroad=parameters[@"avoid_road"];
    request.strategy=[parameters[@"search_strategy"] integerValue];request.ferry=[parameters[@"ferry"] integerValue];
    request.showFieldType=AMapDrivingRouteShowFieldTypeCost | AMapDrivingRouteShowFieldTypePolyline | AMapDrivingRouteShowFieldTypeCities;
    NSMutableArray *waypoints=[NSMutableArray array], *polygons=[NSMutableArray array];
    for(NSDictionary *p in parameters[@"waypoints"])[waypoints addObject:SearchPoint(p)];
    for(NSArray *ring in parameters[@"avoid_polygons"]) {
        NSMutableArray *points=[NSMutableArray array];
        for(NSDictionary *p in ring)[points addObject:SearchPoint(p)];
        [polygons addObject:[AMapGeoPolygon polygonWithPoints:points]];
    }
    request.waypoints=waypoints;request.avoidpolygons=polygons;
    return request;
}
- (void)searchAvoiding:(NSDictionary *)parameters completion:(void (^)(NSArray<NSDictionary<NSString *, id> *> *,NSError *))completion {
    [self prepare];
    AMapDrivingCalRouteSearchRequest *request=[self makeAvoidanceRequest:parameters];
    NSValue *token=[NSValue valueWithNonretainedObject:request];self.searches[token]=[completion copy];
    [self.searchAPI AMapDrivingV2RouteSearch:request];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,20*NSEC_PER_SEC),dispatch_get_main_queue(),^{
        (void)request;
        void (^pending)(NSArray *,NSError *)=self.searches[token];[self.searches removeObjectForKey:token];
        if(pending)pending(nil,SDKError(@"绕行规划超时，请检查网络后重试。"));
    });
}
- (void)onRouteSearchDone:(AMapRouteSearchBaseRequest *)request response:(AMapRouteSearchResponse *)response {
    NSValue *token=[NSValue valueWithNonretainedObject:request];
    void (^completion)(NSArray *,NSError *)=self.searches[token];[self.searches removeObjectForKey:token];if(!completion)return;
    NSMutableArray *routes=[NSMutableArray array];
    for(AMapPath *path in response.route.paths) { NSDictionary *route=SearchRoute(path);if(route)[routes addObject:route]; }
    completion(routes,nil);
}
- (void)resetRouteCandidates {
    self.calculationGeneration++;
    [self.groups removeAllObjects];[self.routeIDs removeAllObjects];
}
- (void)calculate:(NSDictionary *)request completion:(void (^)(NSArray<NSDictionary<NSString *, id> *> *,NSError *))completion {
    [self prepare];
    NSMutableArray *waypoints=[NSMutableArray array];
    for(NSDictionary *p in request[@"waypoints"]) [waypoints addObject:POI(p)];
    AMapNaviPOIInfo *end=POI(request[@"destination"]);
    if([request[@"poi_id"] length]) end.mid=request[@"poi_id"];
    NSUInteger generation=++self.calculationGeneration;
    __block BOOL finished=NO;
    void (^finish)(NSArray *,NSError *)=^(NSArray *routes,NSError *error){ if(finished)return; finished=YES; completion(routes,error); };
    BOOL accepted=[self.manager independentCalculateDriveRouteWithStartPOIInfo:POI(request[@"origin"]) endPOIInfo:end wayPOIInfos:waypoints drivingStrategy:[request[@"strategy"] integerValue] callback:^(AMapNaviRouteGroup *group,NSError *error){
        dispatch_async(dispatch_get_main_queue(),^{
            if(finished)return;
            if(generation!=self.calculationGeneration){finish(nil,SDKError(@"算路请求已被新的请求替代。"));return;}
            if(error || !group.naviRoutes.count){ finish(nil,error ?: SDKError(@"高德没有返回路线。"));return; }
            NSMutableArray *routes=[NSMutableArray array]; NSString *prefix=NSUUID.UUID.UUIDString;
            // Earlier candidates remain usable if later avoidance attempts fail.
            // The service clears this bounded session cache before the next preview.
            for(NSNumber *routeID in group.naviRouteIDs){
                NSString *key=[NSString stringWithFormat:@"%@-%@",prefix,routeID];
                self.groups[key]=group;self.routeIDs[key]=routeID;
                [routes addObject:Route(group.naviRoutes[routeID],key)];
            }
            finish(routes,nil);
        });
    }];
    if(!accepted)finish(nil,SDKError(@"高德未接受算路请求，请检查 iOS Key、Bundle ID 和坐标。"));
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,20*NSEC_PER_SEC),dispatch_get_main_queue(),^{ finish(nil,SDKError(@"高德算路超时，请检查网络。")); });
}
- (BOOL)startRoute:(NSString *)routeID {
    AMapNaviRouteGroup *group=self.groups[routeID]; NSNumber *number=self.routeIDs[routeID];
    if(!group || !number || ![group selectNaviRouteWithRouteID:number.integerValue])return NO;
    [self stop];
    self.routeID=routeID; self.route=group.naviRoute;
    self.generation++;self.awaitingRoute=NO;self.active=YES;self.state=@"acquiring";
    BOOL started=[self.manager startGPSNavi:group];
    if(!started || !self.active){ [self stop];return NO; }
    __weak typeof(self) weakSelf=self;
    self.timer=[NSTimer timerWithTimeInterval:1 repeats:YES block:^(NSTimer *timer){ [weakSelf publish]; }];
    [[NSRunLoop mainRunLoop] addTimer:self.timer forMode:NSRunLoopCommonModes];
    [self publish];return YES;
}
- (void)stop {
    self.active=NO;[self.timer invalidate];self.timer=nil;
    self.lastRouteView=nil;
    [self.manager stopNavi]; self.route=nil;self.info=nil;self.location=nil;self.routeID=@"";self.state=@"cancelled";
    [self publish];
}
- (void)setNetworkAvailable:(BOOL)available { _networkAvailable=available;[self publish]; }
- (void)driveManager:(AMapNaviDriveManager *)manager error:(NSError *)error {
    [self driveManager:manager onCalculateRouteFailure:error];
}
- (void)driveManager:(AMapNaviDriveManager *)manager onCalculateRouteFailure:(NSError *)error routePlanType:(AMapNaviRoutePlanType)type {
    [self driveManager:manager onCalculateRouteFailure:error];
}
- (void)driveManager:(AMapNaviDriveManager *)manager onCalculateRouteFailure:(NSError *)error routePlanType:(AMapNaviRoutePlanType)type calculateRouteResult:(AMapNaviCalculateRouteResult *)result {
    [self driveManager:manager onCalculateRouteFailure:error];
}
- (BOOL)selectPage:(uint8_t)page {
    if(page>3)return NO;self.page=@[@"navigation",@"speed",@"compass",@"music"][page];[self publish];return YES;
}
- (void)driveManager:(AMapNaviDriveManager *)manager updateNaviRoute:(AMapNaviRoute *)route {
    if(!self.active || !route)return;
    NSString *identifier=NSUUID.UUID.UUIDString;
    if(!self.approveRoute || !self.approveRoute(Route(route,identifier))){[self stop];return;}
    self.route=route;self.routeID=identifier;self.info=nil;self.generation++;self.awaitingRoute=NO;self.state=@"navigating";[self.manager resumeNaviSpeech];[self publish];
}
- (void)driveManager:(AMapNaviDriveManager *)manager updateNaviInfo:(AMapNaviInfo *)info {
    if(!self.active || !self.route || self.awaitingRoute || [self.state isEqual:@"arrived"])return; self.info=info;self.state=@"navigating";[self publish];
}
- (void)driveManager:(AMapNaviDriveManager *)manager updateNaviLocation:(AMapNaviLocation *)location {
    if(!self.active)return;self.location=location;[self publish];
}
- (void)driveManagerNeedRecalculateRouteForYaw:(AMapNaviDriveManager *)manager {
    if(!self.active)return;self.state=@"rerouting";self.awaitingRoute=YES;self.info=nil;[self.manager pauseNaviSpeech];[self publish];
}
- (void)driveManagerNeedRecalculateRouteForTrafficJam:(AMapNaviDriveManager *)manager {
    [self driveManagerNeedRecalculateRouteForYaw:manager];
}
- (void)driveManager:(AMapNaviDriveManager *)manager onCalculateRouteFailure:(NSError *)error {
    if(!self.active)return;[self stop];if(self.onFailure)self.onFailure(error.localizedDescription);
}
- (void)driveManagerOnArrivedDestination:(AMapNaviDriveManager *)manager {
    if(!self.active)return;self.state=@"arrived";self.info=nil;[self publish];
}
- (void)publish {
    if(!self.onSnapshot)return;
    MotoNavCoreSnapshot *s=[MotoNavCoreSnapshot new];
    s.stateName=self.state;s.networkName=self.networkAvailable?@"online":@"offline";s.displayPageName=self.page;s.routeID=self.routeID;
    s.nowMs=(uint64_t)(NSDate.date.timeIntervalSince1970*1000);s.routeGeneration=self.generation;
    s.hasDestination=self.active;s.gnssStale=!self.location || !self.location.timestamp || fabs([self.location.timestamp timeIntervalSinceNow])>5;
    s.hasUsableFix=self.active && !s.gnssStale && isfinite(self.location.accuracy) && self.location.accuracy>=0 && self.location.accuracy<=100 && isfinite(self.location.coordinate.longitude) && isfinite(self.location.coordinate.latitude) && fabs(self.location.coordinate.longitude)<=180 && fabs(self.location.coordinate.latitude)<=85;
    // Losing a fix pauses live guidance, not the SDK navigation session. Keep
    // the destination/route identity so the display can wait and resume.
    if(self.active && !s.hasUsableFix && [self.state isEqual:@"navigating"]) s.stateName=@"acquiring";
    s.speedMPS=s.hasUsableFix ? MAX(0,self.location.speed)/3.6:0;s.headingDeg=self.location.heading;
    s.horizontalAccuracyM=self.location.accuracy;s.offRoute=[self.state isEqual:@"rerouting"];s.routeRequestInFlight=s.offRoute;
    s.totalDistanceM=self.route.routeLength;s.remainingDistanceM=self.info ? self.info.routeRemainDistance:self.route.routeLength;
    s.remainingDurationS=(uint32_t)MAX(0,self.info ? self.info.routeRemainTime:self.route.routeTime);
    s.routeProgressM=MAX(0,s.totalDistanceM-s.remainingDistanceM);
    s.hasNextManeuver=self.info && s.hasUsableFix && [self.state isEqual:@"navigating"];
    s.maneuverID=(uint32_t)MAX(0,self.info.currentSegmentIndex+1);s.maneuverTypeName=s.hasNextManeuver ? Maneuver(self.info.iconType):@"unknown";
    s.distanceToManeuverM=s.hasNextManeuver ? self.info.segmentRemainDistance:0;s.roadName=s.hasNextManeuver ? (self.info.nextRoadName ?: @""):@"";
    s.instructionText=@"";s.trafficName=@"unknown";
    s.routeViewOriginLongitudeDeg=self.location.coordinate.longitude;s.routeViewOriginLatitudeDeg=self.location.coordinate.latitude;
    NSMutableArray *points=[NSMutableArray array];
    // Collect only the local forward window; policy always checks the full route separately.
    if(s.hasUsableFix && self.info && !s.offRoute && ![self.state isEqual:@"arrived"]){
        NSInteger seg=self.location.currentSegmentIndex, linkIndex=self.location.currentLinkIndex;
        CLLocation *previous=nil; double distance=0; BOOL done=NO;
        if(seg>=0 && linkIndex>=0) for(NSInteger i=seg;i<(NSInteger)self.route.routeSegments.count && !done;i++){
            AMapNaviSegment *segment=self.route.routeSegments[i];
            for(NSInteger j=(i==seg?linkIndex:0);j<(NSInteger)segment.links.count && !done;j++){
                NSArray *line=segment.links[j].coordinates;
                // SDK point indices are relative to the matched link.
                NSInteger first=(i==seg && j==linkIndex)?MAX(0,self.location.currentPointIndex-1):0;
                for(NSInteger k=first;k<(NSInteger)line.count;k++){
                    AMapNaviPoint *p=line[k];
                    CLLocation *location=[[CLLocation alloc]initWithLatitude:p.latitude longitude:p.longitude];
                    double step=previous ? [location distanceFromLocation:previous]:0;
                    if(previous && step<0.05)continue;
                    if(distance+step>1500 || points.count>=512){done=YES;break;}
                    distance+=step;
                    MotoNavPointValue *point=[MotoNavPointValue new];point.longitudeDeg=p.longitude;point.latitudeDeg=p.latitude;
                    [points addObject:point];previous=location;
                }
            }
        }
    }
    // BLE V1 carries 24 points. Remove the least significant bend, preserving endpoints.
    // This keeps turns much better than truncating or sampling every nth SDK coordinate.
    double lonScale=cos(s.routeViewOriginLatitudeDeg*M_PI/180.0);
    while(points.count>24){
        NSUInteger remove=1; double smallest=DBL_MAX;
        for(NSUInteger i=1;i+1<points.count;i++){
            MotoNavPointValue *a=points[i-1], *b=points[i], *c=points[i+1];
            double dx=(c.longitudeDeg-a.longitudeDeg)*lonScale, dy=c.latitudeDeg-a.latitudeDeg;
            double bx=(b.longitudeDeg-a.longitudeDeg)*lonScale, by=b.latitudeDeg-a.latitudeDeg;
            double norm=dx*dx+dy*dy, t=norm>0?MAX(0,MIN(1,(bx*dx+by*dy)/norm)):0;
            double error=hypot(bx-t*dx,by-t*dy);
            if(error<smallest){smallest=error;remove=i;}
        }
        [points removeObjectAtIndex:remove];
    }
    s.routeViewPoints=points;s.hasRouteView=points.count>=2;
    // Keep the last valid map window for this route while GPS is unavailable.
    // Its origin and heading belong together; an invalid new location must not
    // move the remembered line. Freshness and maneuver flags remain false.
    BOOL canRemember=self.active && !s.offRoute && ![self.state isEqual:@"arrived"] && self.routeID.length>0;
    if(!canRemember || (self.lastRouteView &&
       (![self.lastRouteView.routeID isEqual:self.routeID] || self.lastRouteView.routeGeneration!=self.generation))) {
        self.lastRouteView=nil;
    }
    if(canRemember && s.hasRouteView) {
        self.lastRouteView=s;
    } else if(!s.hasUsableFix && self.lastRouteView) {
        MotoNavCoreSnapshot *last=self.lastRouteView;
        s.routeViewPoints=last.routeViewPoints;s.hasRouteView=YES;
        s.routeViewOriginLongitudeDeg=last.routeViewOriginLongitudeDeg;
        s.routeViewOriginLatitudeDeg=last.routeViewOriginLatitudeDeg;
        s.headingDeg=last.headingDeg;
        s.routeProgressM=last.routeProgressM;
        s.remainingDistanceM=last.remainingDistanceM;s.remainingDurationS=last.remainingDurationS;
    }
    if([self.state isEqual:@"arrived"]){s.remainingDistanceM=0;s.remainingDurationS=0;}
    self.onSnapshot(s);
}
@end
