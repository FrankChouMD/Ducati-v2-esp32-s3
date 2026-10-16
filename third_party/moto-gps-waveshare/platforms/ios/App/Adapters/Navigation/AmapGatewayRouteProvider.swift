import Foundation
import MotoNavigationCore

enum AmapGatewayError: LocalizedError {
    case invalidEndpoint
    case rejected(code: String, message: String, retryable: Bool)
    case invalidResponse
    case motorcyclePolicyUnavailable
    case motorcyclePreviewChanged

    var errorDescription: String? {
        switch self {
        case .invalidEndpoint:
            "路线网关地址无效"
        case let .rejected(code, message, _):
            "\(message)（\(code)）"
        case .invalidResponse:
            "路线网关返回了无法识别的数据"
        case .motorcyclePolicyUnavailable:
            "网关没有返回匹配的南京骑行规则检查，请先更新网关；本次不会退回普通汽车路线。"
        case .motorcyclePreviewChanged:
            "起点已明显变化，请重新规划并核对路线后出发。"
        }
    }

    var isRetryable: Bool {
        if case let .rejected(_, _, retryable) = self { return retryable }
        if case .motorcyclePolicyUnavailable = self { return false }
        if case .motorcyclePreviewChanged = self { return false }
        return true
    }

    var stopsMotorcycleGuidance: Bool {
        if case .motorcyclePolicyUnavailable = self { return true }
        if case .motorcyclePreviewChanged = self { return true }
        if case let .rejected(code, _, retryable) = self {
            return code == "MOTORCYCLE_ROUTE_BLOCKED" || code == "AVOIDANCE_ENDPOINT_CONFLICT" ||
                code == "MOTORCYCLE_AREA_CONFLICT" || (code.hasPrefix("AMAP_") && !retryable)
        }
        return false
    }
}

/// Calls the project's own gateway. It never accepts or transports an AMap API key.
final class AmapGatewayRouteProvider: NavigationRouteProviding, @unchecked Sendable {
    private let baseURL: URL
    private let session: URLSession
    private let encoder = JSONEncoder()
    private let decoder = JSONDecoder()
    private let motorcycleOptions: MotorcycleRouteOptions?

    init(baseURL: URL, session: URLSession = .shared, motorcycleOptions: MotorcycleRouteOptions? = nil) {
        self.baseURL = baseURL
        self.session = session
        self.motorcycleOptions = motorcycleOptions
    }

    private func encodedRequest(_ request: RouteRequest) throws -> Data {
        try encoder.encode(RouteRequest(requestID: request.requestID, origin: request.origin,
            destination: request.destination, isReroute: request.isReroute,
            previousRouteID: request.previousRouteID, destinationPOIID: request.destinationPOIID,
            motorcycleOptions: request.motorcycleOptions ?? motorcycleOptions))
    }

    private func checked(_ route: RoutePlan, request: RouteRequest) throws -> RoutePlan {
        if let options = request.motorcycleOptions ?? motorcycleOptions,
           route.motorcycleAssessment?.matches(options) != true {
            throw AmapGatewayError.motorcyclePolicyUnavailable
        }
        return route
    }

    private func decoded<T: Decodable>(_ type: T.Type, data: Data, request: RouteRequest) throws -> T {
        do { return try decoder.decode(type, from: data) }
        catch {
            if request.motorcycleOptions != nil || motorcycleOptions != nil {
                throw AmapGatewayError.motorcyclePolicyUnavailable
            }
            throw AmapGatewayError.invalidResponse
        }
    }

    func route(for request: RouteRequest) async throws -> RouteEnvelope {
        let endpoint = baseURL.appending(path: "v1/routes")
        guard endpoint.scheme == "https" || endpoint.host == "127.0.0.1" || endpoint.host == "localhost" else {
            throw AmapGatewayError.invalidEndpoint
        }

        var urlRequest = URLRequest(url: endpoint)
        urlRequest.httpMethod = "POST"
        urlRequest.timeoutInterval = 15
        urlRequest.setValue("application/json", forHTTPHeaderField: "Content-Type")
        urlRequest.setValue("application/json", forHTTPHeaderField: "Accept")
        urlRequest.httpBody = try encodedRequest(request)

        let (data, response) = try await session.data(for: urlRequest)
        guard let http = response as? HTTPURLResponse else {
            throw AmapGatewayError.invalidResponse
        }
        guard (200 ..< 300).contains(http.statusCode) else {
            if let failure = try? decoder.decode(GatewayFailureEnvelope.self, from: data) {
                throw AmapGatewayError.rejected(
                    code: failure.error.code,
                    message: failure.error.message, retryable: failure.error.retryable ?? false
                )
            }
            throw AmapGatewayError.invalidResponse
        }
        let envelope = try decoded(RouteEnvelope.self, data: data, request: request)
        guard envelope.protocolVersion == 1, envelope.requestID == request.requestID else {
            throw AmapGatewayError.invalidResponse
        }
        _ = try checked(envelope.route, request: request)
        return envelope
    }

    /// Fetches the candidate routes used by the confirmation screen. New
    /// gateways expose all AMap alternatives at `/v1/route-options`; older
    /// gateways are still usable and fall back to their single `/v1/routes`
    /// result. A missing alternatives endpoint is therefore not allowed to
    /// make normal navigation unavailable during a staggered deployment.
    func routeOptions(for request: RouteRequest) async throws -> [RoutePlan] {
        let endpoint = baseURL.appending(path: "v1/route-options")
        guard endpoint.scheme == "https" || endpoint.host == "127.0.0.1" || endpoint.host == "localhost" else {
            throw AmapGatewayError.invalidEndpoint
        }

        var urlRequest = URLRequest(url: endpoint)
        urlRequest.httpMethod = "POST"
        urlRequest.timeoutInterval = 15
        urlRequest.setValue("application/json", forHTTPHeaderField: "Content-Type")
        urlRequest.setValue("application/json", forHTTPHeaderField: "Accept")
        urlRequest.httpBody = try encodedRequest(request)

        let (data, response) = try await session.data(for: urlRequest)
        guard let http = response as? HTTPURLResponse else {
            throw AmapGatewayError.invalidResponse
        }

        if http.statusCode == 404 || http.statusCode == 405 || http.statusCode == 501 {
            let envelope = try await route(for: request)
            return [envelope.route]
        }

        guard (200 ..< 300).contains(http.statusCode) else {
            if let failure = try? decoder.decode(GatewayFailureEnvelope.self, from: data) {
                throw AmapGatewayError.rejected(
                    code: failure.error.code,
                    message: failure.error.message, retryable: failure.error.retryable ?? false
                )
            }
            throw AmapGatewayError.invalidResponse
        }

        let envelope = try decoded(RouteOptionsEnvelope.self, data: data, request: request)
        guard envelope.protocolVersion == 1,
              envelope.requestID == request.requestID
        else {
            throw AmapGatewayError.invalidResponse
        }

        var seen = Set<String>()
        let routes = envelope.routes
            .filter { route in
                route.polyline.count >= 2 &&
                    route.totalDistanceM > 0 &&
                    route.totalDurationS > 0 &&
                    seen.insert(route.routeID).inserted
            }
            .prefix(3)
        guard !routes.isEmpty else {
            throw AmapGatewayError.invalidResponse
        }
        return try routes.map { try checked($0, request: request) }
    }
}

private struct RouteOptionsEnvelope: Decodable {
    let protocolVersion: Int
    let requestID: UInt32
    let routes: [RoutePlan]

    enum CodingKeys: String, CodingKey {
        case protocolVersion = "protocol_version"
        case requestID = "request_id"
        case routes
    }
}

private struct GatewayFailureEnvelope: Decodable {
    let error: GatewayFailure
}

private struct GatewayFailure: Decodable {
    let code: String
    let message: String
    let retryable: Bool?
}
