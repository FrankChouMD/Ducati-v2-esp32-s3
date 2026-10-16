#import <Foundation/Foundation.h>
#import "MotoNavCoreBridge.h"

NS_ASSUME_NONNULL_BEGIN
/// Main-thread-only boundary for the proprietary SDK. No SDK types escape into Swift.
@interface AMapSDKBridge : NSObject
@property(nonatomic, copy, nullable) void (^onSnapshot)(MotoNavCoreSnapshot *snapshot);
@property(nonatomic, copy, nullable) void (^onFailure)(NSString *message);
@property(nonatomic, copy, nullable) BOOL (^approveRoute)(NSDictionary<NSString *, id> *route);
+ (void)configureKey:(NSString *)key;
- (void)search:(NSString *)query longitude:(double)longitude latitude:(double)latitude
    completion:(void (^)(NSArray<NSDictionary<NSString *, id> *> *_Nullable, NSError *_Nullable))completion;
/// Start a preview session; retain its native route groups across detour attempts.
- (void)resetRouteCandidates;
- (void)calculate:(NSDictionary *)request
    completion:(void (^)(NSArray<NSDictionary<NSString *, id> *> *_Nullable, NSError *_Nullable))completion;
- (void)searchAvoiding:(NSDictionary *)request
    completion:(void (^)(NSArray<NSDictionary<NSString *, id> *> *_Nullable, NSError *_Nullable))completion;
- (BOOL)startRoute:(NSString *)routeID;
- (void)stop;
- (void)setNetworkAvailable:(BOOL)available;
- (BOOL)selectPage:(uint8_t)page;
@end
NS_ASSUME_NONNULL_END
