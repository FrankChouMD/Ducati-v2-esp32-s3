#import <XCTest/XCTest.h>
#import "../App/Adapters/Navigation/AMapSDKBridge.h"

// Data-only doubles exercise the SDK/BLE boundary without SDK services, location or keys.
@interface SDKPointFixture : NSObject
@property double longitude;
@property double latitude;
@end
@implementation SDKPointFixture @end
@interface SDKLocationFixture : NSObject
@property(nonatomic,strong) SDKPointFixture *coordinate;
@property(nonatomic,strong) NSDate *timestamp;
@property double accuracy;
@property double heading;
@property NSInteger speed;
@property int currentSegmentIndex;
@property int currentLinkIndex;
@property int currentPointIndex;
@end
@implementation SDKLocationFixture @end
@interface SDKLinkFixture : NSObject
@property(nonatomic,strong) NSArray *coordinates;
@property(nonatomic,copy) NSString *roadName;
@property NSInteger linkType;
@property NSInteger roadClass;
@property NSInteger trafficStatus;
@property NSInteger length;
@end
@implementation SDKLinkFixture @end
@interface SDKSegmentFixture : NSObject
@property(nonatomic,strong) NSArray *links;
@property NSInteger iconType;
@end
@implementation SDKSegmentFixture @end
@interface SDKRouteFixture : NSObject
@property(nonatomic,strong) NSArray *routeSegments;
@property(nonatomic,strong) NSArray *routeCoordinates;
@property int routeLength;
@property int routeTime;
@end
@implementation SDKRouteFixture @end
@interface SDKRouteGroupFixture : NSObject
@property(nonatomic,strong) NSDictionary *naviRoutes;
@property(nonatomic,strong) NSArray *naviRouteIDs;
@property(nonatomic,strong) SDKRouteFixture *naviRoute;
- (BOOL)selectNaviRouteWithRouteID:(NSInteger)routeID;
@end
@implementation SDKRouteGroupFixture
- (BOOL)selectNaviRouteWithRouteID:(NSInteger)routeID {
    self.naviRoute=self.naviRoutes[@(routeID)];return self.naviRoute!=nil;
}
@end
@interface SDKManagerFixture : NSObject
@property(nonatomic,strong) SDKRouteGroupFixture *nextGroup;
@property(nonatomic,strong) SDKRouteGroupFixture *startedGroup;
@end
@implementation SDKManagerFixture
- (BOOL)independentCalculateDriveRouteWithStartPOIInfo:(id)start endPOIInfo:(id)end wayPOIInfos:(id)waypoints
    drivingStrategy:(NSInteger)strategy callback:(void (^)(id,NSError *))completion {
    completion(self.nextGroup,nil);return YES;
}
- (BOOL)startGPSNavi:(SDKRouteGroupFixture *)group { self.startedGroup=group;return YES; }
- (void)stopNavi {}
@end
@interface SDKInfoFixture : NSObject
@property int routeRemainDistance;
@property int routeRemainTime;
@property int currentSegmentIndex;
@property int segmentRemainDistance;
@property NSInteger iconType;
@property(nonatomic,copy) NSString *nextRoadName;
@end
@implementation SDKInfoFixture @end
@interface SDKSearchStepFixture : NSObject
@property(nonatomic,copy) NSString *polyline;
@property(nonatomic,copy) NSString *road;
@end
@implementation SDKSearchStepFixture @end
@interface SDKSearchPathFixture : NSObject
@property(nonatomic,strong) NSArray *steps;
@property NSInteger distance;
@property NSInteger duration;
@end
@implementation SDKSearchPathFixture @end
@interface SDKSearchRouteFixture : NSObject
@property(nonatomic,strong) NSArray *paths;
@end
@implementation SDKSearchRouteFixture @end
@interface SDKSearchResponseFixture : NSObject
@property(nonatomic,strong) SDKSearchRouteFixture *route;
@end
@implementation SDKSearchResponseFixture @end
@interface AMapSDKBridge (TestCallbacks)
- (void)publish;
- (void)driveManagerNeedRecalculateRouteForYaw:(id)manager;
- (void)driveManagerOnArrivedDestination:(id)manager;
- (void)driveManager:(id)manager updateNaviInfo:(id)info;
- (void)driveManager:(id)manager updateNaviLocation:(id)location;
- (void)driveManager:(id)manager updateNaviRoute:(id)route;
- (id)makeAvoidanceRequest:(NSDictionary *)parameters;
- (void)onRouteSearchDone:(id)request response:(id)response;
@end
@interface AMapSDKSnapshotTests : XCTestCase @end
@implementation AMapSDKSnapshotTests
- (void)testEarlierNativeCandidateSurvivesDetoursAndClearsForTheNextPreview {
    AMapSDKBridge *bridge=[AMapSDKBridge new];SDKManagerFixture *manager=[SDKManagerFixture new];
    [bridge setValue:manager forKey:@"manager"];
    [bridge resetRouteCandidates];
    SDKRouteFixture *route=[SDKRouteFixture new];
    route.routeCoordinates=@[[self point:118.7],[self point:118.9]];
    route.routeSegments=@[];route.routeLength=18000;route.routeTime=1500;
    SDKRouteGroupFixture *first=[SDKRouteGroupFixture new];first.naviRoutes=@{@12:route};first.naviRouteIDs=@[@12];
    SDKRouteGroupFixture *second=[SDKRouteGroupFixture new];second.naviRoutes=@{@12:route};second.naviRouteIDs=@[@12];
    NSDictionary *parameters=@{@"origin":@{@"longitude_deg":@118.7,@"latitude_deg":@31.9},
        @"destination":@{@"longitude_deg":@118.9,@"latitude_deg":@31.9},@"waypoints":@[],@"strategy":@15};
    manager.nextGroup=first;
    __block NSString *firstID;
    XCTestExpectation *initial=[self expectationWithDescription:@"initial candidate"];
    [bridge calculate:parameters completion:^(NSArray *routes,NSError *error){
        XCTAssertNil(error);firstID=routes.firstObject[@"route_id"];[initial fulfill];
    }];
    [self waitForExpectations:@[initial] timeout:3];
    manager.nextGroup=second;
    XCTestExpectation *detour=[self expectationWithDescription:@"detour candidate"];
    [bridge calculate:parameters completion:^(NSArray *routes,NSError *error){
        XCTAssertNil(error);XCTAssertNotEqualObjects(firstID,routes.firstObject[@"route_id"]);[detour fulfill];
    }];
    [self waitForExpectations:@[detour] timeout:3];
    XCTAssertEqual([[bridge valueForKey:@"groups"] count],2u);
    XCTAssertTrue([bridge startRoute:firstID]);
    XCTAssertEqual(manager.startedGroup,first);
    [bridge stop];[bridge resetRouteCandidates];
    XCTAssertEqual([[bridge valueForKey:@"groups"] count],0u);
    XCTAssertFalse([bridge startRoute:firstID]);
}
- (void)testAvoidanceRequestUsesSearchSDKPolygonsAndPreferences {
    AMapSDKBridge *bridge=[AMapSDKBridge new];
    NSDictionary *a=@{@"longitude_deg":@118.7,@"latitude_deg":@31.9};
    NSDictionary *b=@{@"longitude_deg":@118.8,@"latitude_deg":@31.8};
    NSDictionary *c=@{@"longitude_deg":@118.9,@"latitude_deg":@31.9};
    id request=[bridge makeAvoidanceRequest:@{@"origin":a,@"destination":c,@"poi_id":@"B123",@"search_strategy":@43,
        @"ferry":@1,@"waypoints":@[b],@"avoid_polygons":@[@[a,b,c]],@"avoid_road":@"测试高架路"}];
    XCTAssertEqualObjects([request valueForKey:@"strategy"],@43);
    XCTAssertEqualObjects([request valueForKey:@"ferry"],@1);
    XCTAssertEqualObjects([request valueForKey:@"destinationId"],@"B123");
    XCTAssertEqualObjects([request valueForKey:@"avoidroad"],@"测试高架路");
    XCTAssertEqualWithAccuracy([[request valueForKeyPath:@"origin.longitude"] doubleValue],118.7,0.000001);
    NSArray *polygons=[request valueForKey:@"avoidpolygons"], *waypoints=[request valueForKey:@"waypoints"];
    XCTAssertEqual(polygons.count,1u);XCTAssertEqual(waypoints.count,1u);
    XCTAssertEqual([[polygons.firstObject valueForKey:@"points"] count],3u);
    XCTAssertEqualWithAccuracy([[waypoints.firstObject valueForKey:@"latitude"] doubleValue],31.8,0.000001);
    NSUInteger fields=[[request valueForKey:@"showFieldType"] unsignedIntegerValue];
    XCTAssertNotEqual(fields & (1u<<5),0u); // full polyline requested
    XCTAssertNotEqual(fields & (1u<<1),0u); // duration/cost requested
}
- (void)testFullLinkTypesDistinguishElevatedRoadsFromBridgesAndSurfaceExpressways {
    AMapSDKBridge *bridge=[AMapSDKBridge new];
    SDKLinkFixture *link=[SDKLinkFixture new];
    link.coordinates=@[[self point:118.7],[self point:118.8]];link.length=1000;link.roadClass=6;link.roadName=@"";
    SDKSegmentFixture *segment=[SDKSegmentFixture new];segment.links=@[link];
    SDKRouteFixture *route=[SDKRouteFixture new];route.routeCoordinates=link.coordinates;
    route.routeSegments=@[segment];route.routeLength=1000;route.routeTime=100;
    __block NSDictionary *raw;
    bridge.approveRoute=^BOOL(NSDictionary *candidate){raw=candidate;return NO;};
    for(NSNumber *type in @[@0,@3,@4,@(-1)]) {
        link.linkType=type.integerValue;
        [bridge setValue:@YES forKey:@"active"];
        [bridge driveManager:nil updateNaviRoute:route];
        XCTAssertEqualObjects(raw[@"sdk_has_elevated"],@([type isEqual:@4]));
        XCTAssertEqualObjects(raw[@"sdk_elevated_checked"],@(![type isEqual:@(-1)]));
        XCTAssertEqualObjects(raw[@"sdk_steps"][0][@"link_type"],type);
        XCTAssertEqualObjects(raw[@"sdk_has_highway"],@NO);
        XCTAssertEqualObjects([bridge valueForKey:@"active"],@NO); // Denied reroute stops guidance immediately.
    }
    SDKSegmentFixture *missing=[SDKSegmentFixture new];missing.links=@[];
    link.linkType=0;route.routeSegments=@[segment,missing];
    [bridge setValue:@YES forKey:@"active"];
    [bridge driveManager:nil updateNaviRoute:route];
    XCTAssertEqualObjects(raw[@"sdk_elevated_checked"],@NO);
    // One late elevated link must be visible even beyond the BLE preview window.
    NSMutableArray *links=[NSMutableArray array];
    for(int i=0;i<150;i++)[links addObject:link];
    SDKLinkFixture *late=[SDKLinkFixture new];late.linkType=4;late.coordinates=link.coordinates;
    [links addObject:late];segment.links=links;route.routeSegments=@[segment];
    [bridge setValue:@YES forKey:@"active"];
    [bridge driveManager:nil updateNaviRoute:route];
    XCTAssertEqual([raw[@"sdk_steps"] count],151u);
    XCTAssertEqualObjects(raw[@"sdk_has_elevated"],@YES);
    XCTAssertEqualObjects(raw[@"sdk_elevated_checked"],@YES);
}
- (void)testSearchSeedKeepsFullGeometryAndRejectsMissingFragments {
    AMapSDKBridge *bridge=[AMapSDKBridge new];NSObject *request=[NSObject new];
    SDKSearchStepFixture *step=[SDKSearchStepFixture new];step.road=@"绕行路";step.polyline=@"118.7,31.9;118.8,31.8;118.9,31.9";
    SDKSearchPathFixture *path=[SDKSearchPathFixture new];path.steps=@[step];path.distance=18000;path.duration=1500;
    SDKSearchRouteFixture *route=[SDKSearchRouteFixture new];route.paths=@[path];
    SDKSearchResponseFixture *response=[SDKSearchResponseFixture new];response.route=route;
    NSMutableDictionary *pending=[bridge valueForKey:@"searches"];
    __block NSArray *result;
    void (^complete)(NSArray *,NSError *)=^(NSArray *value,NSError *error){XCTAssertNil(error);result=value;};
    NSValue *token=[NSValue valueWithNonretainedObject:request];pending[token]=complete;
    [bridge onRouteSearchDone:request response:response];
    XCTAssertEqual(result.count,1u);XCTAssertEqual([result[0][@"polyline"] count],3u);
    XCTAssertEqualObjects(result[0][@"provider"],@"amap_ios_search_sdk");
    SDKSearchStepFixture *missing=[SDKSearchStepFixture new];missing.polyline=@"";missing.road=@"unknown";
    path.steps=@[step,missing];pending[token]=complete;
    [bridge onRouteSearchDone:request response:response];XCTAssertEqual(result.count,0u);
}
- (SDKPointFixture *)point:(double)lon {
    SDKPointFixture *point=[SDKPointFixture new];point.longitude=lon;point.latitude=31.9;return point;
}
- (AMapSDKBridge *)bridgeWithLocation:(SDKLocationFixture *)location {
    AMapSDKBridge *bridge=[AMapSDKBridge new];
    SDKLinkFixture *past=[SDKLinkFixture new];past.coordinates=@[[self point:118.6],[self point:118.61]];
    SDKLinkFixture *current=[SDKLinkFixture new];NSMutableArray *points=[NSMutableArray array];
    for(int i=0;i<150;i++)[points addObject:[self point:118.7+i*0.00005]];
    current.coordinates=points;
    SDKSegmentFixture *segment=[SDKSegmentFixture new];segment.links=@[past,current];
    SDKRouteFixture *route=[SDKRouteFixture new];route.routeSegments=@[segment];route.routeLength=2000;route.routeTime=200;
    SDKInfoFixture *info=[SDKInfoFixture new];info.routeRemainDistance=1000;info.routeRemainTime=100;
    info.iconType=3;info.segmentRemainDistance=80;info.nextRoadName=@"test road";
    [bridge setValue:route forKey:@"route"];[bridge setValue:info forKey:@"info"];
    [bridge setValue:location forKey:@"location"];[bridge setValue:@YES forKey:@"active"];
    [bridge setValue:@"sdk-test" forKey:@"routeID"];[bridge setValue:@"navigating" forKey:@"state"];
    return bridge;
}
- (SDKLocationFixture *)location {
    SDKLocationFixture *location=[SDKLocationFixture new];location.coordinate=[self point:118.7001];
    location.timestamp=NSDate.date;location.accuracy=5;location.speed=72;
    location.currentSegmentIndex=0;location.currentLinkIndex=1;location.currentPointIndex=2;
    return location;
}
- (void)testMatchedLinkWindowAndSpeedUseBLEUnitsAndCapacity {
    AMapSDKBridge *bridge=[self bridgeWithLocation:self.location];
    __block MotoNavCoreSnapshot *snapshot;bridge.onSnapshot=^(MotoNavCoreSnapshot *s){snapshot=s;};[bridge publish];
    XCTAssertTrue(snapshot.hasUsableFix);XCTAssertEqualWithAccuracy(snapshot.speedMPS,20,0.001);
    XCTAssertTrue(snapshot.hasRouteView);XCTAssertLessThanOrEqual(snapshot.routeViewPoints.count,24u);
    XCTAssertEqualWithAccuracy(snapshot.routeViewPoints.firstObject.longitudeDeg,118.70005,0.000001);
    XCTAssertEqualWithAccuracy(snapshot.routeViewPoints.lastObject.longitudeDeg,118.70745,0.000001);
    XCTAssertEqualObjects(snapshot.maneuverTypeName,@"right");
}
- (void)testStaleLocationWithoutHistoryDoesNotInventRouteGeometry {
    SDKLocationFixture *location=self.location;location.timestamp=[NSDate dateWithTimeIntervalSinceNow:-10];
    AMapSDKBridge *bridge=[self bridgeWithLocation:location];
    __block MotoNavCoreSnapshot *snapshot;bridge.onSnapshot=^(MotoNavCoreSnapshot *s){snapshot=s;};[bridge publish];
    XCTAssertTrue(snapshot.gnssStale);XCTAssertFalse(snapshot.hasUsableFix);
    XCTAssertFalse(snapshot.hasNextManeuver);XCTAssertFalse(snapshot.hasRouteView);XCTAssertEqual(snapshot.speedMPS,0);
    XCTAssertTrue(snapshot.hasDestination);XCTAssertEqualObjects(snapshot.stateName,@"acquiring");
    XCTAssertEqualObjects(snapshot.routeID,@"sdk-test");
}
- (void)testLiveNavigationWaitsThroughFiveSecondFixGapAndResumesWithoutRestart {
    SDKLocationFixture *location=self.location;
    AMapSDKBridge *bridge=[self bridgeWithLocation:location];
    __block MotoNavCoreSnapshot *snapshot;bridge.onSnapshot=^(MotoNavCoreSnapshot *s){snapshot=s;};
    [bridge publish];
    XCTAssertEqualObjects(snapshot.stateName,@"navigating");XCTAssertTrue(snapshot.hasRouteView);
    MotoNavCoreSnapshot *last=snapshot;
    NSString *routeID=snapshot.routeID;uint32_t generation=snapshot.routeGeneration;
    location.timestamp=[NSDate dateWithTimeIntervalSinceNow:-6];
    for(int tick=0;tick<3;tick++) {
        [bridge publish];
        XCTAssertEqualObjects(snapshot.stateName,@"acquiring");XCTAssertTrue(snapshot.hasDestination);
        XCTAssertEqualObjects(snapshot.routeID,routeID);XCTAssertEqual(snapshot.routeGeneration,generation);
        XCTAssertTrue(snapshot.hasRouteView);XCTAssertFalse(snapshot.hasNextManeuver);
        XCTAssertFalse(snapshot.hasUsableFix);XCTAssertTrue(snapshot.gnssStale);
        XCTAssertEqualObjects(snapshot.routeViewPoints,last.routeViewPoints);
        XCTAssertEqual(snapshot.routeViewOriginLongitudeDeg,last.routeViewOriginLongitudeDeg);
        XCTAssertEqual(snapshot.routeViewOriginLatitudeDeg,last.routeViewOriginLatitudeDeg);
        XCTAssertEqual(snapshot.speedMPS,0);
        XCTAssertFalse(snapshot.routeRequestInFlight);XCTAssertEqual(snapshot.totalDistanceM,2000);
    }
    // The next SDK fix resumes the same route and current guidance by itself.
    [bridge driveManager:nil updateNaviLocation:self.location];
    XCTAssertEqualObjects(snapshot.stateName,@"navigating");XCTAssertTrue(snapshot.hasDestination);
    XCTAssertEqualObjects(snapshot.routeID,routeID);XCTAssertEqual(snapshot.routeGeneration,generation);
    XCTAssertTrue(snapshot.hasRouteView);XCTAssertTrue(snapshot.hasNextManeuver);
    [bridge stop];
    XCTAssertFalse(snapshot.hasDestination);XCTAssertFalse(snapshot.hasRouteView);
    XCTAssertEqualObjects(snapshot.stateName,@"cancelled");
}
- (void)testInvalidAndMissingFixKeepTheLastMapPoseUntilANewFixArrives {
    SDKLocationFixture *location=self.location;location.heading=90;
    AMapSDKBridge *bridge=[self bridgeWithLocation:location];
    __block MotoNavCoreSnapshot *snapshot;bridge.onSnapshot=^(MotoNavCoreSnapshot *s){snapshot=s;};
    [bridge publish];MotoNavCoreSnapshot *last=snapshot;
    SDKLocationFixture *bad=self.location;bad.accuracy=500;bad.coordinate.longitude=0;bad.heading=270;
    SDKInfoFixture *info=[bridge valueForKey:@"info"];info.routeRemainDistance=100;
    [bridge driveManager:nil updateNaviLocation:bad];
    XCTAssertFalse(snapshot.hasUsableFix);XCTAssertTrue(snapshot.hasRouteView);
    XCTAssertEqualObjects(snapshot.routeViewPoints,last.routeViewPoints);
    XCTAssertEqual(snapshot.headingDeg,last.headingDeg);
    XCTAssertEqual(snapshot.routeViewOriginLongitudeDeg,last.routeViewOriginLongitudeDeg);
    XCTAssertEqual(snapshot.routeProgressM,last.routeProgressM);
    XCTAssertEqual(snapshot.remainingDistanceM,last.remainingDistanceM);
    [bridge driveManager:nil updateNaviLocation:nil];
    XCTAssertTrue(snapshot.gnssStale);XCTAssertTrue(snapshot.hasRouteView);
    XCTAssertEqualObjects(snapshot.routeViewPoints,last.routeViewPoints);
    XCTAssertEqual(snapshot.routeViewOriginLatitudeDeg,last.routeViewOriginLatitudeDeg);
    XCTAssertFalse(snapshot.hasNextManeuver);

    SDKLocationFixture *fresh=self.location;fresh.currentPointIndex=20;
    fresh.coordinate.longitude=118.701;fresh.heading=100;
    [bridge driveManager:nil updateNaviLocation:fresh];
    XCTAssertTrue(snapshot.hasUsableFix);XCTAssertTrue(snapshot.hasNextManeuver);
    XCTAssertEqual(snapshot.headingDeg,100);
    XCTAssertEqualWithAccuracy(snapshot.routeViewOriginLongitudeDeg,118.701,0.000001);
    XCTAssertGreaterThan(snapshot.routeViewPoints.firstObject.longitudeDeg,last.routeViewPoints.firstObject.longitudeDeg);
    MotoNavCoreSnapshot *updated=snapshot;
    [bridge driveManager:nil updateNaviLocation:nil];
    XCTAssertEqualObjects(snapshot.routeViewPoints,updated.routeViewPoints);
    XCTAssertEqual(snapshot.headingDeg,100);
}
- (void)testRememberedWindowNeverCrossesRouteIdentityOrGeneration {
    for(NSString *key in @[@"routeID",@"generation"]) {
        AMapSDKBridge *bridge=[self bridgeWithLocation:self.location];
        __block MotoNavCoreSnapshot *snapshot;bridge.onSnapshot=^(MotoNavCoreSnapshot *s){snapshot=s;};
        [bridge publish];XCTAssertTrue(snapshot.hasRouteView);
        id previous=[bridge valueForKey:key];
        [bridge setValue:([key isEqual:@"routeID"] ? (id)@"another-route" : (id)@2) forKey:key];
        [bridge driveManager:nil updateNaviLocation:nil];
        XCTAssertFalse(snapshot.hasRouteView);
        [bridge setValue:previous forKey:key];[bridge publish];
        XCTAssertFalse(snapshot.hasRouteView); // Discarded history cannot reappear.
    }
}
- (void)testRerouteAndArrivalSuppressLateOldInstructions {
    AMapSDKBridge *bridge=[self bridgeWithLocation:self.location];id oldInfo=[bridge valueForKey:@"info"];
    __block MotoNavCoreSnapshot *snapshot;bridge.onSnapshot=^(MotoNavCoreSnapshot *s){snapshot=s;};
    [bridge publish];XCTAssertTrue(snapshot.hasRouteView);
    [bridge driveManagerNeedRecalculateRouteForYaw:nil];[bridge driveManager:nil updateNaviInfo:oldInfo];
    XCTAssertEqualObjects(snapshot.stateName,@"rerouting");XCTAssertFalse(snapshot.hasNextManeuver);XCTAssertFalse(snapshot.hasRouteView);
    [bridge setValue:@NO forKey:@"awaitingRoute"];
    [bridge driveManager:nil updateNaviInfo:oldInfo];XCTAssertTrue(snapshot.hasRouteView);
    [bridge driveManagerOnArrivedDestination:nil];[bridge driveManager:nil updateNaviInfo:oldInfo];
    XCTAssertEqualObjects(snapshot.stateName,@"arrived");XCTAssertEqual(snapshot.remainingDistanceM,0);
    XCTAssertFalse(snapshot.hasNextManeuver);
    [bridge driveManager:nil updateNaviLocation:nil];XCTAssertFalse(snapshot.hasRouteView);
}
@end
