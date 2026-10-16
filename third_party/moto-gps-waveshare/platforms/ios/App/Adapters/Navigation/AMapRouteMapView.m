#import "AMapRouteMapView.h"
#import <AMapNaviKit/AMapNaviKit.h>

@interface AMapRouteMapView () <MAMapViewDelegate>
@property(nonatomic,strong) MAMapView *map;
@property(nonatomic,copy) NSString *selected;
@end
@implementation AMapRouteMapView
- (instancetype)initWithFrame:(CGRect)frame {
    if((self=[super initWithFrame:frame])){
        _map=[[MAMapView alloc]initWithFrame:self.bounds]; _map.delegate=self;
        _map.autoresizingMask=UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;
        _map.mapType=MAMapTypeStandardNight; _map.showTraffic=YES;
        _map.showsCompass=NO; _map.showsScale=NO;
        _map.scrollEnabled=NO; _map.zoomEnabled=NO; _map.rotateEnabled=NO; _map.rotateCameraEnabled=NO;
        [self addSubview:_map];
    }
    return self;
}
- (void)showRoutes:(NSArray<NSDictionary<NSString *,id> *> *)routes selected:(NSString *)selected
             areas:(NSArray<NSDictionary<NSString *,id> *> *)areas endpoints:(NSArray<NSDictionary<NSString *,id> *> *)endpoints {
    self.selected=selected;
    [self.map removeOverlays:self.map.overlays]; [self.map removeAnnotations:self.map.annotations];
    for(NSDictionary *area in areas){
        NSArray *points=area[@"points"]; if(points.count<3)continue;
        CLLocationCoordinate2D *coords=calloc(points.count,sizeof(CLLocationCoordinate2D));
        for(NSUInteger i=0;i<points.count;i++)coords[i]=CLLocationCoordinate2DMake([points[i][@"lat"] doubleValue],[points[i][@"lon"] doubleValue]);
        MAPolygon *polygon=[MAPolygon polygonWithCoordinates:coords count:points.count];free(coords);
        polygon.title=[area[@"active"] boolValue]?@"active":@"reference";
        [self.map addOverlay:polygon];
    }
    NSMutableArray *routeLines=[NSMutableArray array];
    for(NSDictionary *route in routes){
        NSArray *points=route[@"points"];if(points.count<2)continue;
        CLLocationCoordinate2D *coords=calloc(points.count,sizeof(CLLocationCoordinate2D));
        for(NSUInteger i=0;i<points.count;i++)coords[i]=CLLocationCoordinate2DMake([points[i][@"lat"] doubleValue],[points[i][@"lon"] doubleValue]);
        MAPolyline *line=[MAPolyline polylineWithCoordinates:coords count:points.count];free(coords);
        line.title=route[@"id"];[self.map addOverlay:line];[routeLines addObject:line];
    }
    for(NSDictionary *point in endpoints){
        MAPointAnnotation *annotation=[MAPointAnnotation new];
        annotation.coordinate=CLLocationCoordinate2DMake([point[@"lat"] doubleValue],[point[@"lon"] doubleValue]);
        annotation.title=point[@"title"];[self.map addAnnotation:annotation];
    }
    [self.map showOverlays:routeLines edgePadding:UIEdgeInsetsMake(38,30,42,30) animated:NO];
}
- (MAOverlayRenderer *)mapView:(MAMapView *)mapView rendererForOverlay:(id<MAOverlay>)overlay {
    if([overlay isKindOfClass:MAPolygon.class]){
        MAPolygon *polygon=(MAPolygon *)overlay; MAPolygonRenderer *r=[[MAPolygonRenderer alloc]initWithPolygon:polygon];
        BOOL active=[polygon.title isEqual:@"active"];UIColor *color=active?UIColor.orangeColor:UIColor.grayColor;
        r.fillColor=[color colorWithAlphaComponent:active?0.18:0.08];r.strokeColor=color;r.lineWidth=1.5;return r;
    }
    if([overlay isKindOfClass:MAPolyline.class]){
        MAPolyline *line=(MAPolyline *)overlay;MAPolylineRenderer *r=[[MAPolylineRenderer alloc]initWithPolyline:line];
        BOOL selected=[line.title isEqual:self.selected];r.lineWidth=selected?7:5;
        r.strokeColor=selected?[UIColor colorWithRed:232.0/255 green:0 blue:22.0/255 alpha:1]:UIColor.grayColor;
        return r;
    }
    return nil;
}
- (MAAnnotationView *)mapView:(MAMapView *)mapView viewForAnnotation:(id<MAAnnotation>)annotation {
    if(![annotation isKindOfClass:MAPointAnnotation.class])return nil;
    MAAnnotationView *view=[mapView dequeueReusableAnnotationViewWithIdentifier:@"endpoint"];
    if(!view)view=[[MAAnnotationView alloc]initWithAnnotation:annotation reuseIdentifier:@"endpoint"];
    view.annotation=annotation;view.canShowCallout=YES;
    UIImageSymbolConfiguration *config=[UIImageSymbolConfiguration configurationWithPointSize:24 weight:UIImageSymbolWeightBold];
    view.image=[[UIImage systemImageNamed:[annotation.title isEqual:@"当前位置"]?@"location.circle.fill":@"flag.circle.fill" withConfiguration:config] imageWithTintColor:UIColor.redColor renderingMode:UIImageRenderingModeAlwaysOriginal];
    return view;
}
@end
