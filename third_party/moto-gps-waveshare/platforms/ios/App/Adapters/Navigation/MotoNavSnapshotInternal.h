#import "MotoNavCoreBridge.h"

@interface MotoNavCoreSnapshot ()
@property(nonatomic, readwrite, copy) NSString *stateName;
@property(nonatomic, readwrite, copy) NSString *networkName;
@property(nonatomic, readwrite, copy) NSString *displayPageName;
@property(nonatomic, readwrite) BOOL hasDestination;
@property(nonatomic, readwrite) BOOL hasUsableFix;
@property(nonatomic, readwrite) BOOL gnssStale;
@property(nonatomic, readwrite) BOOL offRoute;
@property(nonatomic, readwrite) BOOL routeRequestInFlight;
@property(nonatomic, readwrite) BOOL trafficRequestInFlight;
@property(nonatomic, readwrite) double speedMPS;
@property(nonatomic, readwrite) double headingDeg;
@property(nonatomic, readwrite) double horizontalAccuracyM;
@property(nonatomic, readwrite) double crossTrackDistanceM;
@property(nonatomic, readwrite) uint16_t speedLimitKPH;
@property(nonatomic, readwrite) BOOL hasRouteView;
@property(nonatomic, readwrite) double routeViewOriginLongitudeDeg;
@property(nonatomic, readwrite) double routeViewOriginLatitudeDeg;
@property(nonatomic, readwrite, copy) NSArray<MotoNavPointValue *> *routeViewPoints;
@property(nonatomic, readwrite, copy) NSString *routeID;
@property(nonatomic, readwrite) double routeProgressM;
@property(nonatomic, readwrite) double totalDistanceM;
@property(nonatomic, readwrite) double remainingDistanceM;
@property(nonatomic, readwrite) uint32_t remainingDurationS;
@property(nonatomic, readwrite) BOOL hasNextManeuver;
@property(nonatomic, readwrite) uint32_t maneuverID;
@property(nonatomic, readwrite, copy) NSString *maneuverTypeName;
@property(nonatomic, readwrite) double distanceToManeuverM;
@property(nonatomic, readwrite, copy) NSString *roadName;
@property(nonatomic, readwrite, copy) NSString *instructionText;
@property(nonatomic, readwrite) uint8_t roundaboutExit;
@property(nonatomic, readwrite, copy) NSString *trafficName;
@property(nonatomic, readwrite) uint64_t nowMs;
@property(nonatomic, readwrite) uint32_t routeGeneration;
@end
