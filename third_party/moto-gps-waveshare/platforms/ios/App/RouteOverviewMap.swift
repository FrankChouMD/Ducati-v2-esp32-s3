import MotoNavigationCore
import SwiftUI

/// SDK geometry and map both use GCJ-02. Only CoreLocation endpoints are transformed.
struct RouteOverviewMap: UIViewRepresentable {
    let candidates: [RoutePreviewCandidate]
    let selectedID: String?
    let origin: WGS84Point?
    let destination: WGS84Point

    func makeCoordinator() -> Coordinator { Coordinator() }
    func makeUIView(context: Context) -> UIView {
        // Also prevents offline UI tests from initializing a network SDK.
        guard AMapSDKSettings.ready else { return UIView() }
        return AMapRouteMapView(frame: .zero)
    }
    func updateUIView(_ view: UIView, context: Context) {
        guard let map = view as? AMapRouteMapView else { return }
        let signature = candidates.map(\.id).joined(separator: "|") + "::" + (selectedID ?? "")
        guard context.coordinator.signature != signature else { return }
        context.coordinator.signature = signature
        func point(_ p: GCJ02Point) -> [String: Double] { ["lon":p.longitudeDeg,"lat":p.latitudeDeg] }
        let routes: [[String: Any]] = candidates.sorted {
            ($0.id == selectedID ? 1 : 0) < ($1.id == selectedID ? 1 : 0)
        }.map { ["id":$0.id,"points":$0.route.polyline.map(point)] }
        var endpoints: [[String: Any]] = []
        if let origin {
            let p = ChinaCoordinateTransform.wgs84ToGCJ02(origin)
            endpoints.append(["lon":p.longitudeDeg,"lat":p.latitudeDeg,"title":"当前位置"])
        }
        let end = ChinaCoordinateTransform.wgs84ToGCJ02(destination)
        endpoints.append(["lon":end.longitudeDeg,"lat":end.latitudeDeg,"title":"目的地"])
        map.showRoutes(routes, selected: selectedID ?? "", areas: [], endpoints: endpoints)
    }
    final class Coordinator { var signature = "" }
}
