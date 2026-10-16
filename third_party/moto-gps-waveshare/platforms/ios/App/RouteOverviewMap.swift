import MapKit
import MotoNavigationCore
import SwiftUI

// Ducati V2 personal UI adaptation, 2026-10-16: dark map and red route accents.
struct RouteOverviewMap: UIViewRepresentable {
    @Environment(\.accessibilityReduceMotion) private var reduceMotion

    let candidates: [RoutePreviewCandidate]
    let selectedID: String?
    let origin: WGS84Point?
    let destination: WGS84Point

    private var restrictionAreas: [MotorcycleRestrictionArea] {
        candidates.first(where: { $0.id == selectedID })?.route.motorcycleAssessment?.restrictionAreas ?? []
    }

    func makeCoordinator() -> Coordinator {
        Coordinator()
    }

    func makeUIView(context: Context) -> MKMapView {
        let mapView = MKMapView(frame: .zero)
        mapView.delegate = context.coordinator
        mapView.registerForTraitChanges([
            UITraitUserInterfaceStyle.self,
            UITraitAccessibilityContrast.self
        ]) { [weak coordinator = context.coordinator] (mapView: MKMapView, _: UITraitCollection) in
            coordinator?.refreshRouteAppearance(on: mapView)
        }
        mapView.mapType = .mutedStandard
        mapView.overrideUserInterfaceStyle = .dark
        mapView.pointOfInterestFilter = .excludingAll
        mapView.showsCompass = false
        mapView.showsScale = false
        mapView.showsTraffic = true
        // This map is the route-selection overview, not a free-roaming map.
        // Let vertical gestures reach the enclosing SwiftUI ScrollView so all
        // route choices remain reachable instead of accidentally panning the
        // selected route out of view.
        mapView.isScrollEnabled = false
        mapView.isZoomEnabled = false
        mapView.isRotateEnabled = false
        mapView.isPitchEnabled = false
        return mapView
    }

    func updateUIView(_ mapView: MKMapView, context: Context) {
        context.coordinator.selectedID = selectedID
        let signature = candidates.map(\.id).joined(separator: "|") +
            "::" + (selectedID ?? "") + "::" + restrictionAreas.map {
                "\($0.id):\($0.dataVersion):\($0.appliesToRegistration)"
            }.joined(separator: "|")
        guard context.coordinator.signature != signature else { return }
        context.coordinator.signature = signature

        mapView.removeOverlays(mapView.overlays)
        mapView.removeAnnotations(mapView.annotations)

        for area in restrictionAreas where area.isValid {
            let coordinates = area.polygon.map { point in
                let wgs84 = ChinaCoordinateTransform.gcj02ToWGS84(point)
                return CLLocationCoordinate2D(latitude: wgs84.latitudeDeg, longitude: wgs84.longitudeDeg)
            }
            let polygon = MKPolygon(coordinates: coordinates, count: coordinates.count)
            polygon.title = area.appliesToRegistration ? "motu-active" : "motu-reference"
            mapView.addOverlay(polygon, level: .aboveRoads)
        }

        let ordered = candidates.sorted { lhs, rhs in
            (lhs.id == selectedID ? 1 : 0) < (rhs.id == selectedID ? 1 : 0)
        }
        var allCoordinates: [CLLocationCoordinate2D] = []
        for candidate in ordered {
            let coordinates = candidate.route.polyline.map { point in
                let wgs84 = ChinaCoordinateTransform.gcj02ToWGS84(point)
                return CLLocationCoordinate2D(
                    latitude: wgs84.latitudeDeg,
                    longitude: wgs84.longitudeDeg
                )
            }
            guard coordinates.count >= 2 else { continue }
            let polyline = MKPolyline(coordinates: coordinates, count: coordinates.count)
            polyline.title = candidate.id
            mapView.addOverlay(polyline, level: .aboveRoads)
            allCoordinates.append(contentsOf: coordinates)
        }

        if let origin {
            let annotation = MKPointAnnotation()
            annotation.coordinate = CLLocationCoordinate2D(
                latitude: origin.latitudeDeg,
                longitude: origin.longitudeDeg
            )
            annotation.title = "当前位置"
            annotation.subtitle = "moto-start"
            mapView.addAnnotation(annotation)
            allCoordinates.append(annotation.coordinate)
        }

        let destinationAnnotation = MKPointAnnotation()
        destinationAnnotation.coordinate = CLLocationCoordinate2D(
            latitude: destination.latitudeDeg,
            longitude: destination.longitudeDeg
        )
        destinationAnnotation.title = "目的地"
        destinationAnnotation.subtitle = "moto-finish"
        mapView.addAnnotation(destinationAnnotation)
        allCoordinates.append(destinationAnnotation.coordinate)

        guard !allCoordinates.isEmpty else { return }
        var visibleRect = MKMapRect.null
        for coordinate in allCoordinates {
            let point = MKMapPoint(coordinate)
            visibleRect = visibleRect.union(
                MKMapRect(x: point.x, y: point.y, width: 0.1, height: 0.1)
            )
        }
        mapView.setVisibleMapRect(
            visibleRect,
            edgePadding: UIEdgeInsets(top: 38, left: 30, bottom: 42, right: 30),
            animated: context.coordinator.hasPresentedRoute && !reduceMotion
        )
        context.coordinator.hasPresentedRoute = true
    }

    final class Coordinator: NSObject, MKMapViewDelegate {
        var signature = ""
        var selectedID: String?
        var hasPresentedRoute = false

        func mapView(_ mapView: MKMapView, rendererFor overlay: MKOverlay) -> MKOverlayRenderer {
            if let polygon = overlay as? MKPolygon {
                let renderer = MKPolygonRenderer(polygon: polygon)
                let active = polygon.title == "motu-active"
                let color = active ? UIColor(red: 1, green: 0.64, blue: 0.16, alpha: 1) : UIColor.gray
                renderer.fillColor = color.withAlphaComponent(active ? 0.18 : 0.08)
                renderer.strokeColor = color.withAlphaComponent(0.85)
                renderer.lineWidth = 1.5
                return renderer
            }
            guard let polyline = overlay as? MKPolyline else {
                return MKOverlayRenderer(overlay: overlay)
            }
            let renderer = MKPolylineRenderer(polyline: polyline)
            configure(renderer, for: polyline, traits: mapView.traitCollection)
            return renderer
        }

        func refreshRouteAppearance(on mapView: MKMapView) {
            // Overlay renderers draw resolved colors. Refresh their strokes when
            // appearance changes even if the route IDs have stayed the same.
            for overlay in mapView.overlays {
                guard let polyline = overlay as? MKPolyline,
                      let renderer = mapView.renderer(for: overlay) as? MKPolylineRenderer
                else { continue }
                configure(renderer, for: polyline, traits: mapView.traitCollection)
                renderer.setNeedsDisplay()
            }
        }

        private func configure(
            _ renderer: MKPolylineRenderer,
            for polyline: MKPolyline,
            traits: UITraitCollection
        ) {
            let isSelected = polyline.title == selectedID
            let color = isSelected
                ? UIColor(red: 232.0 / 255, green: 0, blue: 22.0 / 255, alpha: 1)
                : UIColor(white: 0.58, alpha: 1)
            renderer.strokeColor = color.resolvedColor(with: traits)
            renderer.lineWidth = isSelected ? 7 : 5
            renderer.lineCap = .round
            renderer.lineJoin = .round
        }

        func mapView(_ mapView: MKMapView, viewFor annotation: MKAnnotation) -> MKAnnotationView? {
            guard let point = annotation as? MKPointAnnotation else { return nil }
            let identifier = "route-endpoint"
            let view = (mapView.dequeueReusableAnnotationView(withIdentifier: identifier) as? MKMarkerAnnotationView)
                ?? MKMarkerAnnotationView(annotation: point, reuseIdentifier: identifier)
            view.annotation = point
            let isStart = point.subtitle == "moto-start"
            view.markerTintColor = isStart ? .darkGray
                : UIColor(red: 232.0 / 255, green: 0, blue: 22.0 / 255, alpha: 1)
            view.glyphImage = UIImage(systemName: isStart ? "location.fill" : "flag.fill")
            view.glyphTintColor = .white
            view.subtitleVisibility = .hidden
            view.displayPriority = .required
            return view
        }
    }
}
