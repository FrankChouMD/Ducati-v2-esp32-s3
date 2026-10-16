#import <UIKit/UIKit.h>
NS_ASSUME_NONNULL_BEGIN
/// Construct only after SDK privacy consent and key configuration.
@interface AMapRouteMapView : UIView
- (void)showRoutes:(NSArray<NSDictionary<NSString *,id> *> *)routes
         selected:(NSString *)selected
            areas:(NSArray<NSDictionary<NSString *,id> *> *)areas
        endpoints:(NSArray<NSDictionary<NSString *,id> *> *)endpoints;
@end
NS_ASSUME_NONNULL_END
