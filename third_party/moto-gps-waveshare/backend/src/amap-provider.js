import {
  transformAmapRouteOptionsV2,
  transformAmapRouteV2,
} from "./amap-transformer.js";
import { wgs84ToGcj02 } from "./coordinates.js";
import {
  applyMotorcycleParameters, assessMotorcycleRoute, motorcycleStrategy,
  validateMotorcycleEndpoints, NANJING_POLICY,
} from "./motorcycle-policy.js";
import {
  fetchGetWithTransientRetry,
  isTransientUpstreamStatus,
} from "./upstream-retry.js";

const DEFAULT_ENDPOINT = "https://restapi.amap.com/v5/direction/driving";

export class AmapProviderError extends Error {
  constructor(code, message, { retryable = true } = {}) {
    super(message);
    this.name = "AmapProviderError";
    this.code = code;
    this.retryable = retryable;
  }
}

function formatProviderPoint(point) {
  return `${Number(point.longitude_deg).toFixed(6)},${Number(point.latitude_deg).toFixed(6)}`;
}

export function buildAmapDrivingUrl(request, { key, endpoint = DEFAULT_ENDPOINT, strategy } = {}) {
  if (typeof key !== "string" || key.trim() === "") {
    throw new AmapProviderError("SERVER_MISCONFIGURED", "AMap server key is not configured", {
      retryable: false,
    });
  }

  const origin = wgs84ToGcj02(request.origin);
  const destination = wgs84ToGcj02(request.destination);
  const url = new URL(endpoint);
  if (url.protocol !== "https:") {
    throw new AmapProviderError("SERVER_MISCONFIGURED", "AMap endpoint must use HTTPS", {
      retryable: false,
    });
  }
  url.searchParams.set("key", key);
  url.searchParams.set("origin", formatProviderPoint(origin));
  url.searchParams.set("destination", formatProviderPoint(destination));
  url.searchParams.set("strategy", "32");
  url.searchParams.set("show_fields", "cost,polyline,navi,tmcs");
  if (request.motorcycle_options) {
    applyMotorcycleParameters(url, request.motorcycle_options);
    url.searchParams.set("show_fields", "cost,polyline,navi,tmcs,cities");
  }
  if (strategy !== undefined) url.searchParams.set("strategy", String(strategy));
  if (request.destination_poi_id) {
    url.searchParams.set("destination_id", request.destination_poi_id);
  }
  return url;
}

export function createAmapProvider({
  key,
  fetchImpl = globalThis.fetch,
  endpoint = DEFAULT_ENDPOINT,
  timeoutMs = 8000,
  maxAttempts = 3,
  retryDelayMs = 100,
  clock = Date.now,
} = {}) {
  if (typeof fetchImpl !== "function") {
    throw new TypeError("fetchImpl must be a function");
  }

  async function fetchRoutePayload(request, { signal, strategy } = {}) {
      const url = buildAmapDrivingUrl(request, { key, endpoint, strategy });
      const deadlineSignal = AbortSignal.timeout(timeoutMs);
      const signals = [deadlineSignal];
      if (signal) signals.push(signal);
      const requestSignal = AbortSignal.any(signals);

      let response;
      try {
        response = await fetchGetWithTransientRetry(fetchImpl, url, {
          headers: { Accept: "application/json" },
          signal: requestSignal,
          maxAttempts,
          retryDelayMs,
        });
      } catch (error) {
        const code =
          deadlineSignal.aborted || error?.name === "TimeoutError"
            ? "PROVIDER_TIMEOUT"
            : "PROVIDER_UNAVAILABLE";
        throw new AmapProviderError(code, "AMap route service is unavailable");
      }

      if (!response.ok) {
        throw new AmapProviderError(
          "PROVIDER_HTTP_ERROR",
          `AMap returned HTTP ${response.status}`,
          {
            retryable:
              isTransientUpstreamStatus(response.status) || response.status === 429,
          },
        );
      }

      let payload;
      try {
        payload = await response.json();
      } catch {
        throw new AmapProviderError("PROVIDER_INVALID_JSON", "AMap returned invalid JSON");
      }
      return payload;
  }

  async function checkedMotorcycleRoutes(request, options = {}) {
    validateMotorcycleEndpoints(request);
    const settings = request.motorcycle_options;
    const primary = motorcycleStrategy(settings);
    const alternate = settings.avoid_highways ? (settings.avoid_tolls ? 42 : 35) : (settings.avoid_tolls ? 36 : 32);
    const signals = [AbortSignal.timeout(timeoutMs)];
    if (options.signal) signals.push(options.signal);
    const signal = AbortSignal.any(signals);
    const rejected = new Set();
    // Both policy attempts share one deadline; each keeps the same regions,
    // waypoints, registration and preferences, including during rerouting.
    for (const strategy of [primary, alternate]) {
      const payload = await fetchRoutePayload(request, { signal, strategy });
      const routes = transformAmapRouteOptionsV2(payload, { generatedAtMs: clock() });
      const accepted = [];
      for (const [index, route] of routes.entries()) {
        const { violations, assessment } = assessMotorcycleRoute(route, payload.route.paths[index], request, clock());
        if (violations.length) violations.forEach(name => rejected.add(name));
        else accepted.push({ ...route, motorcycle_assessment: assessment });
      }
      if (accepted.length) return accepted;
    }
    throw new AmapProviderError("MOTORCYCLE_ROUTE_BLOCKED",
      `候选路线涉及禁行通道或避让区：${[...rejected].join("、")}。请调整途经点或目的地。`, { retryable: false });
  }

  return {
    motorcyclePolicyVersion: NANJING_POLICY.version,
    async planRoute(request, options = {}) {
      if (request.motorcycle_options) return (await checkedMotorcycleRoutes(request, options))[0];
      const payload = await fetchRoutePayload(request, options);
      return transformAmapRouteV2(payload, { generatedAtMs: clock() });
    },

    async planRouteOptions(request, options = {}) {
      if (request.motorcycle_options) return checkedMotorcycleRoutes(request, options);
      const payload = await fetchRoutePayload(request, options);
      return transformAmapRouteOptionsV2(payload, { generatedAtMs: clock() });
    },
  };
}
