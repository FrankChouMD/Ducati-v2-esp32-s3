import SwiftUI

// Ducati V2 personal UI adaptation, 2026-10-16. Original navigation/session
// behavior is retained from MOTO GPS; see the repository NOTICE for attribution.
enum MotoScreen: Hashable {
    case routePreview
    case activeNavigation
}

struct ContentView: View {
    @ObservedObject var model: AppModel
    @Environment(\.dynamicTypeSize) private var dynamicTypeSize
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    @Environment(\.openURL) private var openURL
    @FocusState private var searchFocused: Bool
    @State private var showsDeviceDetails = false
    @State private var showsMapDownloads = false
    @State private var showsDataUse = false
    @State private var showsGatewaySettings = false
    @State private var showsRidingSettings = false

    var body: some View {
        NavigationStack(path: navigationPath) {
            homeScreen
                .navigationDestination(for: MotoScreen.self) { screen in
                    switch screen {
                    case .routePreview: routePreviewScreen
                    case .activeNavigation: activeNavigationScreen
                    }
                }
        }
        .ducatiAppTheme()
        .onChange(of: model.destinationQuery) { _, _ in model.destinationQueryDidChange() }
        .onChange(of: model.selectedPlace) { _, place in
            if place != nil { searchFocused = false }
        }
        .onChange(of: model.isNavigationActive) { _, active in
            if active { searchFocused = false }
        }
        .sheet(isPresented: $showsDeviceDetails) { deviceDetails.ducatiAppTheme() }
        .sheet(isPresented: $showsMapDownloads) {
            MapDownloadsView(
                store: model.surroundingMap,
                gatewayBaseURL: model.mapGatewayBaseURL,
                route: model.mapDownloadRoute,
                destinationName: model.selectedPlace?.name
            ).ducatiAppTheme()
        }
        .sheet(isPresented: $showsDataUse) { DataUseView().ducatiAppTheme() }
        .sheet(isPresented: $showsGatewaySettings) { GatewaySettingsView(model: model).ducatiAppTheme() }
        .sheet(isPresented: $showsRidingSettings) { MotorcycleSettingsView(model: model) }
    }

    // Derive the stack from the session. System Back and the edge gesture
    // continue to clear a preview or stop the active ride through the model.
    private var navigationPath: Binding<[MotoScreen]> {
        Binding(
            get: {
                if model.isNavigationActive { return [.activeNavigation] }
                if model.selectedPlace != nil { return [.routePreview] }
                return []
            },
            set: { path in
                if model.isNavigationActive, !path.contains(.activeNavigation) {
                    model.stopNavigation()
                } else if model.selectedPlace != nil, !path.contains(.routePreview) {
                    model.clearDestination()
                }
            }
        )
    }

    // MARK: - Destination search

    private var homeScreen: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 22) {
                DucatiBrand()
                if dynamicTypeSize.isAccessibilitySize {
                    Text("选择目的地").font(.title2.weight(.bold))
                } else {
                    VStack(alignment: .leading, spacing: 10) {
                        Text("下一程，\n即刻出发。")
                            .font(.largeTitle.weight(.bold))
                            .fixedSize(horizontal: false, vertical: true)
                        Text("选好路线，把前方交给车机。")
                            .font(.subheadline).foregroundStyle(DucatiTheme.secondary)
                    }
                }
                searchField
                if !model.destinationQuery.isEmpty {
                    searchResultsSection
                } else {
                    if !model.isGatewayConfigured { gatewaySetupButton }
                    ridingSettingsButton
                    deviceSummaryButton
                    recentPlacesSection
                    VStack(alignment: .leading, spacing: 12) {
                        DucatiSectionTitle(title: "骑行工具", index: "TOOLS")
                        if dynamicTypeSize.isAccessibilitySize {
                            mapToolButton
                            demoButton
                        } else {
                            HStack(alignment: .top, spacing: 12) {
                                mapToolButton
                                demoButton
                            }.fixedSize(horizontal: false, vertical: true)
                        }
                    }
                    HStack(spacing: 20) {
                        Button { showsGatewaySettings = true } label: {
                            Label("网关设置", systemImage: "gearshape")
                                .font(.caption).frame(minHeight: 44)
                        }.accessibilityIdentifier("gateway-settings-button")
                        Button { showsDataUse = true } label: {
                            Label("隐私与数据", systemImage: "hand.raised")
                                .font(.caption).frame(minHeight: 44)
                        }.accessibilityIdentifier("privacy-data-button")
                    }
                    .foregroundStyle(DucatiTheme.secondary)
                    Text("POWERED BY MOTO GPS · MALER X")
                        .font(DucatiTheme.label(11)).tracking(1.4)
                        .foregroundStyle(DucatiTheme.secondary)
                }
            }
            .padding(.horizontal, 20).padding(.top, 18).padding(.bottom, 28)
        }
        .scrollDismissesKeyboard(.interactively)
        .ducatiScreen()
        .navigationTitle("出发")
        .navigationBarTitleDisplayMode(.inline)
        .toolbar {
            ToolbarItem(placement: .topBarLeading) {
                Button { showsGatewaySettings = true } label: {
                    Image(systemName: "gearshape").frame(minWidth: 44, minHeight: 44)
                }
                .foregroundStyle(DucatiTheme.primary)
                .accessibilityLabel("网关设置")
                .accessibilityIdentifier("gateway-settings-toolbar")
            }
            ToolbarItem(placement: .topBarTrailing) { deviceToolbarButton }
            ToolbarItemGroup(placement: .keyboard) {
                Spacer()
                Button("完成") { searchFocused = false }
            }
        }
    }

    private var gatewaySetupButton: some View {
        Button { showsGatewaySettings = true } label: {
            HStack(alignment: .top, spacing: 12) {
                Image(systemName: "network").foregroundStyle(DucatiTheme.warning)
                VStack(alignment: .leading, spacing: 5) {
                    Text("设置导航网关").font(.subheadline.weight(.semibold))
                    Text("填写网关地址，开启地点搜索与路线规划。")
                        .font(.caption).foregroundStyle(DucatiTheme.secondary)
                }
                Spacer(minLength: 0)
                Image(systemName: "chevron.right").font(.caption)
            }
            .frame(maxWidth: .infinity, alignment: .leading).ducatiCard()
        }
        .buttonStyle(.plain)
        .accessibilityIdentifier("gateway-setup-button")
    }

    private var searchField: some View {
        HStack(spacing: 12) {
            Image(systemName: "magnifyingglass")
                .foregroundStyle(DucatiTheme.accent).accessibilityHidden(true)
            TextField("搜索地点或地址", text: $model.destinationQuery)
                .font(.body)
                .accessibilityIdentifier("destination-search-field")
                .accessibilityLabel("搜索目的地")
                .focused($searchFocused)
                .submitLabel(.search)
                .textInputAutocapitalization(.never)
                .autocorrectionDisabled()
                .onSubmit {
                    model.submitDestinationSearch()
                    searchFocused = false
                }
            if !model.destinationQuery.isEmpty {
                Button {
                    model.clearDestination()
                    searchFocused = true
                } label: {
                    Image(systemName: "xmark.circle.fill")
                        .foregroundStyle(DucatiTheme.secondary)
                        .frame(minWidth: 44, minHeight: 44)
                }
                .buttonStyle(.plain).accessibilityLabel("清空搜索")
            }
        }
        .frame(minHeight: 24).ducatiCard()
    }

    private var searchResultsSection: some View {
        VStack(alignment: .leading, spacing: 12) {
            DucatiSectionTitle(title: "搜索结果", index: "DESTINATION")
            if model.isSearchingPlaces {
                HStack(spacing: 12) {
                    ProgressView().tint(DucatiTheme.accent)
                    Text("正在搜索…").foregroundStyle(DucatiTheme.secondary)
                }.frame(maxWidth: .infinity, minHeight: 56).ducatiCard()
            } else if !model.placeResults.isEmpty {
                VStack(spacing: 0) {
                    ForEach(Array(model.placeResults.enumerated()), id: \.offset) { index, place in
                        if index > 0 { Divider().overlay(DucatiTheme.border) }
                        Button { select(place) } label: { placeRow(place, symbol: "mappin.circle.fill") }
                            .buttonStyle(.plain).accessibilityIdentifier("place-result-\(index)")
                    }
                }.ducatiCard()
            } else {
                ContentUnavailableView {
                    Label(model.placeSearchFailure == nil ? "输入地点名称" : "暂无结果",
                          systemImage: "magnifyingglass")
                } description: {
                    Text(model.placeSearchFailure ?? "至少输入两个字，例如“奥体中心”。")
                } actions: {
                    if model.placeSearchFailure != nil {
                        Button("重新搜索", action: model.submitDestinationSearch)
                    }
                }.ducatiCard()
            }
            Text(model.searchScopeText).font(.caption).foregroundStyle(DucatiTheme.secondary)
            if model.searchLocationStatus == .permissionDenied {
                Button("前往设置开启定位", action: openSystemSettings).frame(minHeight: 44)
            }
        }
    }

    private var recentPlacesSection: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                DucatiSectionTitle(title: "最近搜索", index: "RECENT")
                if !model.recentPlaces.isEmpty {
                    Button("清空", action: model.clearRecentPlaces)
                        .font(.caption).frame(minWidth: 44, minHeight: 44)
                        .foregroundStyle(DucatiTheme.secondary)
                        .accessibilityLabel("清空最近搜索")
                }
            }
            if model.recentPlaces.isEmpty {
                HStack(spacing: 12) {
                    Image(systemName: "mappin.and.ellipse").font(.title2)
                        .foregroundStyle(DucatiTheme.secondary)
                    VStack(alignment: .leading, spacing: 5) {
                        Text("下一站，想去哪里？").font(.subheadline.weight(.medium))
                        Text("搜索地点或地址，选好路线就出发。")
                            .font(.caption).foregroundStyle(DucatiTheme.secondary)
                    }
                }.frame(maxWidth: .infinity, alignment: .leading).ducatiCard()
            } else {
                VStack(spacing: 0) {
                    ForEach(Array(model.recentPlaces.enumerated()), id: \.offset) { index, place in
                        if index > 0 { Divider().overlay(DucatiTheme.border) }
                        Button { select(place) } label: { placeRow(place, symbol: "clock") }
                            .buttonStyle(.plain).accessibilityIdentifier("recent-place-\(index)")
                    }
                }.ducatiCard()
            }
        }
    }

    private func placeRow(_ place: PlaceSearchResult, symbol: String) -> some View {
        HStack(spacing: 12) {
            Image(systemName: symbol).font(.title3)
                .foregroundStyle(symbol == "clock" ? DucatiTheme.secondary : DucatiTheme.accent)
                .frame(width: 26).accessibilityHidden(true)
            VStack(alignment: .leading, spacing: 5) {
                Text(place.name).font(.body.weight(.medium))
                    .fixedSize(horizontal: false, vertical: true)
                Text(placeSubtitle(place)).font(.caption).foregroundStyle(DucatiTheme.secondary)
                    .lineLimit(dynamicTypeSize.isAccessibilitySize ? nil : 2)
            }
            Spacer(minLength: 0)
            Image(systemName: "chevron.right").font(.caption.weight(.semibold))
                .foregroundStyle(DucatiTheme.secondary).accessibilityHidden(true)
        }
        .padding(.vertical, 12).frame(minHeight: 52).contentShape(Rectangle())
    }

    private func select(_ place: PlaceSearchResult) {
        searchFocused = false
        model.selectPlace(place)
    }

    private var mapToolButton: some View {
        Button { showsMapDownloads = true } label: {
            DucatiToolTile(title: "离线地图", subtitle: "提前保存沿途地图", symbol: "map")
        }
        .buttonStyle(.plain)
        .accessibilityLabel("地图与离线下载")
        .accessibilityIdentifier("map-downloads-button")
    }

    private var demoButton: some View {
        Button {
            searchFocused = false
            model.startDemoNavigation()
        } label: {
            DucatiToolTile(title: "演示导航", subtitle: "先体验一次导航", symbol: "play.circle")
        }
        .buttonStyle(.plain)
        .accessibilityIdentifier("demo-navigation-button")
    }

    // MARK: - Route selection

    private var ridingSettingsButton: some View {
        Button { showsRidingSettings = true } label: {
            HStack(spacing: 12) {
                Image(systemName: "slider.horizontal.3").foregroundStyle(DucatiTheme.accent)
                VStack(alignment: .leading, spacing: 5) {
                    Text("南京骑行").font(.subheadline.weight(.semibold))
                    Text("\(model.ridingOptions.registration.title) · \(model.ridingOptions.avoidHighways ? "避开高速" : "允许高速算路")")
                        .font(.caption).foregroundStyle(DucatiTheme.secondary)
                        .fixedSize(horizontal: false, vertical: true)
                    Text(model.ridingOptions.registration == .nanjingUrban
                         ? "摩途禁外摩区 · 当前登记类型不适用" : "摩途禁外摩区 · 自动避让")
                        .font(.caption2).foregroundStyle(DucatiTheme.warning)
                }
                Spacer(minLength: 0)
                Image(systemName: "chevron.right").font(.caption).foregroundStyle(DucatiTheme.secondary)
            }.frame(minHeight: 40).ducatiCard()
        }.buttonStyle(.plain).accessibilityIdentifier("riding-settings-button")
    }

    private var mapDownloadsButton: some View {
        Button { showsMapDownloads = true } label: {
            HStack(spacing: 12) {
                Image(systemName: "arrow.down.to.line").foregroundStyle(DucatiTheme.accent)
                Text("下载沿途离线地图").font(.subheadline)
                Spacer()
                Image(systemName: "chevron.right").font(.caption).foregroundStyle(DucatiTheme.secondary)
            }.frame(minHeight: 44)
        }
        .buttonStyle(.plain)
        .accessibilityIdentifier("map-downloads-button")
    }

    private var routePreviewScreen: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                DucatiBrand(caption: "ROUTE SELECTION")
                ridingSettingsButton
                if let place = model.selectedPlace {
                    VStack(alignment: .leading, spacing: 8) {
                        Label("从我的位置出发", systemImage: "location.fill")
                            .font(.caption).foregroundStyle(DucatiTheme.secondary)
                        Text(place.name).font(.title.weight(.bold))
                            .fixedSize(horizontal: false, vertical: true)
                        Text(placeSubtitle(place)).font(.caption).foregroundStyle(DucatiTheme.secondary)
                    }
                    if model.isPlanningRoutePreview {
                        HStack(spacing: 14) {
                            ProgressView().tint(DucatiTheme.accent)
                            VStack(alignment: .leading, spacing: 4) {
                                Text("正在规划路线")
                                Text("获取当前位置与路况…").font(.caption).foregroundStyle(DucatiTheme.secondary)
                            }
                        }
                        .frame(maxWidth: .infinity, minHeight: 90, alignment: .leading)
                        .ducatiCard().accessibilityIdentifier("route-preview-loading")
                    } else if model.hasRoutePreview {
                        RouteOverviewMap(
                            candidates: model.routePreviewCandidates,
                            selectedID: model.selectedRoutePreviewID,
                            origin: model.routePreviewOrigin,
                            destination: place.location
                        )
                        .frame(height: dynamicTypeSize.isAccessibilitySize ? 220 : 250)
                        .clipShape(RoundedRectangle(cornerRadius: 18))
                        .overlay { RoundedRectangle(cornerRadius: 18).strokeBorder(DucatiTheme.border) }
                        .accessibilityIdentifier("route-preview-map")
                        .accessibilityLabel("前往\(place.name)的路线全览")
                        VStack(alignment: .leading, spacing: 10) {
                            DucatiSectionTitle(title: "选择路线", index: "SELECT ROUTE")
                            ForEach(model.routePreviewCandidates) { candidate in routeOptionRow(candidate) }
                            Text("高德驾车路线 · 预计时间会随路况变化")
                                .font(.caption2).foregroundStyle(DucatiTheme.secondary)
                        }
                        if let assessment = model.selectedRoutePreview?.route.motorcycleAssessment {
                            MotorcycleAssessmentCard(assessment: assessment, review: $model.ridingReviewConfirmed)
                        }
                        mapDownloadsButton
                    } else if let failure = model.routePreviewFailure {
                        ContentUnavailableView {
                            Label("无法规划路线", systemImage: "exclamationmark.triangle")
                        } description: { Text(failure) } actions: {
                            if model.searchLocationStatus == .permissionDenied {
                                Button("前往设置", action: openSystemSettings)
                            }
                        }.ducatiCard()
                    }
                    if let failure = model.navigationFailure { failureMessage(failure) }
                }
            }.padding(20)
        }
        .ducatiScreen().navigationTitle("路线").navigationBarTitleDisplayMode(.inline)
        .toolbar {
            ToolbarItem(placement: .topBarTrailing) {
                Button("更换") {
                    model.clearDestination()
                    searchFocused = true
                }
                .foregroundStyle(DucatiTheme.primary)
                .accessibilityLabel("更换目的地").accessibilityIdentifier("destination-change-button")
            }
        }
        .safeAreaInset(edge: .bottom, spacing: 0) { navigationActionBar }
    }

    private func routeOptionRow(_ candidate: RoutePreviewCandidate) -> some View {
        let selected = candidate.id == model.selectedRoutePreviewID
        return Button {
            withAnimation(reduceMotion ? nil : .easeInOut(duration: 0.2)) {
                model.selectRoutePreview(candidate.id)
            }
        } label: {
            HStack(spacing: 12) {
                VStack(alignment: .leading, spacing: 8) {
                    Text(candidate.title).font(.caption.weight(.semibold))
                        .foregroundStyle(selected ? DucatiTheme.primary : DucatiTheme.secondary)
                    ViewThatFits(in: .horizontal) {
                        HStack(alignment: .firstTextBaseline, spacing: 12) {
                            routeDuration(candidate)
                            routeDistance(candidate)
                        }
                        VStack(alignment: .leading, spacing: 4) {
                            routeDuration(candidate)
                            routeDistance(candidate)
                        }
                    }
                    Label(candidate.trafficSummary, systemImage: "car.side")
                        .font(.caption).foregroundStyle(trafficTint(candidate))
                }.frame(maxWidth: .infinity, alignment: .leading)
                Image(systemName: selected ? "checkmark.circle.fill" : "circle")
                    .font(.title3).foregroundStyle(selected ? DucatiTheme.accent : DucatiTheme.border)
                    .accessibilityHidden(true)
            }.ducatiCard(selected: selected).contentShape(RoundedRectangle(cornerRadius: 18))
        }
        .buttonStyle(.plain)
        .accessibilityIdentifier("route-option-\(candidate.ordinal)")
        .accessibilityLabel("\(candidate.title)，\(candidate.durationText)，\(candidate.distanceText)，\(candidate.trafficSummary)")
        .accessibilityAddTraits(selected ? .isSelected : [])
    }

    private func routeDuration(_ candidate: RoutePreviewCandidate) -> some View {
        DucatiReadout(value: candidate.durationText, size: 34, unitSize: 14)
    }

    private func routeDistance(_ candidate: RoutePreviewCandidate) -> some View {
        Text(candidate.distanceText).font(DucatiTheme.label(20))
            .foregroundStyle(DucatiTheme.secondary).fixedSize(horizontal: false, vertical: true)
    }

    // MARK: - Ride in progress

    private var activeNavigationScreen: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 22) {
                DucatiBrand(caption: model.isDemoActive ? "DEMO SESSION" : "RIDE IN PROGRESS")
                VStack(alignment: .leading, spacing: 18) {
                    HStack {
                        Image(systemName: navigationSymbol)
                            .font(.system(size: 30, weight: .medium))
                            .foregroundStyle(model.navigationFailure == nil ? DucatiTheme.accent : DucatiTheme.warning)
                            .accessibilityHidden(true)
                        Spacer()
                        DucatiStatus(title: model.isDemoActive ? "模拟数据" : locationStatus,
                                     color: model.isDemoActive ? DucatiTheme.warning : navigationStateColor)
                    }
                    Text(navigationTitle).font(.largeTitle.weight(.bold))
                        .fixedSize(horizontal: false, vertical: true)
                    Text(navigationDetail).font(.subheadline)
                        .foregroundStyle(DucatiTheme.secondary)
                        .fixedSize(horizontal: false, vertical: true)
                }.frame(maxWidth: .infinity, alignment: .leading).ducatiCard()
                VStack(alignment: .leading, spacing: 10) {
                    DucatiSectionTitle(title: model.isDemoActive ? "演示目的地" : "目的地", index: "DESTINATION")
                    Label(model.activeDestinationName, systemImage: "flag.checkered")
                        .font(.title3.weight(.semibold))
                        .fixedSize(horizontal: false, vertical: true)
                }
                VStack(spacing: 0) {
                    Rectangle().fill(DucatiTheme.accent).frame(height: 2).accessibilityHidden(true)
                    if dynamicTypeSize.isAccessibilitySize {
                        VStack(alignment: .leading, spacing: 18) {
                            rideMetric("剩余时间", value: remainingDuration)
                            rideMetric("剩余路程", value: remainingDistance)
                        }.padding(20)
                    } else {
                        HStack(alignment: .top, spacing: 20) {
                            rideMetric("剩余时间", value: remainingDuration)
                            rideMetric("剩余路程", value: remainingDistance)
                        }.padding(20)
                    }
                }
                .background(DucatiTheme.surface, in: RoundedRectangle(cornerRadius: 18))
                .clipShape(RoundedRectangle(cornerRadius: 18))
                if !model.isDemoActive, let assessment = model.activeMotorcycleAssessment {
                    MotorcycleAssessmentCard(assessment: assessment)
                }
                VStack(alignment: .leading, spacing: 12) {
                    DucatiSectionTitle(title: "导航状态", index: "LIVE STATUS")
                    deviceSummaryButton
                    VStack(spacing: 10) {
                        statusRow("手机定位", symbol: "location", value: locationStatus,
                                  color: model.navigation.hasUsableFix ? DucatiTheme.ready : DucatiTheme.secondary)
                        Divider().overlay(DucatiTheme.border)
                        statusRow("路况", symbol: "car.side", value: trafficStatus, color: DucatiTheme.secondary)
                        Divider().overlay(DucatiTheme.border)
                        SurroundingMapStatusRow(store: model.surroundingMap)
                    }.ducatiCard()
                    Button("管理离线地图") { showsMapDownloads = true }.font(.subheadline).frame(minHeight: 44)
                }
                if model.isDemoActive {
                    Label("演示中的位置与行驶过程为模拟数据。", systemImage: "info.circle")
                        .font(.caption).foregroundStyle(DucatiTheme.warning)
                    Text("道路数据 © OpenStreetMap contributors")
                        .font(.caption2).foregroundStyle(DucatiTheme.secondary)
                }
            }.padding(20)
        }
        .ducatiScreen()
        .navigationTitle(model.isDemoActive ? "演示导航" : "导航中")
        .navigationBarTitleDisplayMode(.inline)
        .toolbar { ToolbarItem(placement: .topBarTrailing) { deviceToolbarButton } }
        .safeAreaInset(edge: .bottom, spacing: 0) { navigationActionBar }
    }

    private var navigationStateColor: Color {
        model.navigationFailure != nil ? DucatiTheme.warning :
            (model.navigation.hasUsableFix ? DucatiTheme.ready : DucatiTheme.secondary)
    }

    private func rideMetric(_ title: String, value: String) -> some View {
        VStack(alignment: .leading, spacing: 6) {
            DucatiReadout(value: value)
            Text(title).font(.caption).foregroundStyle(DucatiTheme.secondary)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .accessibilityElement(children: .ignore).accessibilityLabel("\(title)，\(value)")
    }

    private var navigationActionBar: some View {
        VStack(spacing: 10) {
            Button(action: model.toggleNavigation) {
                HStack(spacing: 10) {
                    if model.isPlanningRoutePreview {
                        ProgressView().tint(DucatiTheme.primary)
                    } else {
                        Image(systemName: model.isNavigationActive ? "stop.fill" : "location.fill")
                    }
                    Text(model.primaryActionTitle).fixedSize(horizontal: false, vertical: true)
                }.padding(.horizontal, 16).padding(.vertical, 4)
            }
            .buttonStyle(DucatiPrimaryButtonStyle())
            .disabled(!model.isNavigationActive && (model.selectedPlace == nil || model.isPlanningRoutePreview ||
                (model.selectedRoutePreview != nil && !model.canStartNavigation)))
            .accessibilityIdentifier("primary-navigation-action")
        }
        .padding(.horizontal, 20).padding(.top, 14).padding(.bottom, 10)
        .background(DucatiTheme.background)
        .overlay(alignment: .top) { Rectangle().fill(DucatiTheme.border).frame(height: 1) }
    }

    // MARK: - Device sheet

    private var deviceToolbarButton: some View {
        Button {
            searchFocused = false
            showsDeviceDetails = true
        } label: { Image(systemName: "display").frame(minWidth: 44, minHeight: 44) }
        .foregroundStyle(DucatiTheme.primary)
        .accessibilityLabel("我的车机").accessibilityValue(deviceStatus)
        .accessibilityIdentifier("device-details-button")
    }

    private var deviceSummaryButton: some View {
        Button {
            searchFocused = false
            showsDeviceDetails = true
        } label: {
            Group {
                if dynamicTypeSize.isAccessibilitySize {
                    VStack(alignment: .leading, spacing: 14) {
                        deviceSummaryLabel
                        deviceStatusBadge
                    }
                } else {
                    HStack(spacing: 14) {
                        deviceSummaryLabel
                        Spacer(minLength: 0)
                        deviceStatusBadge
                    }
                }
            }.frame(minHeight: 40).ducatiCard().contentShape(RoundedRectangle(cornerRadius: 18))
        }
        .buttonStyle(.plain).accessibilityIdentifier("device-summary-button")
    }

    private var deviceSummaryLabel: some View {
        HStack(spacing: 14) {
            Image(systemName: "display").font(.system(size: 28, weight: .light))
                .foregroundStyle(DucatiTheme.primary).frame(width: 38).accessibilityHidden(true)
            VStack(alignment: .leading, spacing: 5) {
                Text("我的车机").font(.subheadline.weight(.semibold))
                Text(model.deviceReady ? "导航同步已就绪" : "连接后自动同步导航")
                    .font(.caption).foregroundStyle(DucatiTheme.secondary)
            }.fixedSize(horizontal: false, vertical: true)
        }
    }

    private var deviceStatusBadge: some View {
        DucatiStatus(title: deviceStatus, color: model.deviceReady ? DucatiTheme.ready : DucatiTheme.secondary)
    }

    private var deviceDetails: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: 22) {
                    DucatiBrand(caption: "CONNECTED DISPLAY")
                    VStack(spacing: 18) {
                        Image(systemName: "display").font(.system(size: 64, weight: .ultraLight))
                            .foregroundStyle(DucatiTheme.primary).padding(32)
                            .background(DucatiTheme.surface, in: RoundedRectangle(cornerRadius: 30))
                            .overlay { RoundedRectangle(cornerRadius: 30).strokeBorder(DucatiTheme.accent) }
                            .accessibilityHidden(true)
                        Text(deviceName).font(.title2.weight(.semibold))
                        DucatiStatus(title: deviceStatus, color: model.deviceReady ? DucatiTheme.ready : DucatiTheme.secondary)
                        Text(deviceDetail).font(.subheadline).foregroundStyle(DucatiTheme.secondary)
                            .multilineTextAlignment(.center)
                    }.frame(maxWidth: .infinity).padding(.vertical, 16)
                    VStack(spacing: 12) {
                        statusRow("连接状态", symbol: "antenna.radiowaves.left.and.right", value: deviceStatus,
                                  color: model.deviceReady ? DucatiTheme.ready : DucatiTheme.secondary)
                        Divider().overlay(DucatiTheme.border)
                        statusRow("搜索位置", symbol: "location", value: searchLocationStatus, color: DucatiTheme.secondary)
                    }.ducatiCard()
                    Button(connectionActionTitle, action: model.toggleDeviceConnection)
                        .buttonStyle(DucatiPrimaryButtonStyle()).accessibilityIdentifier("device-connection-action")
                    if case .bluetoothUnavailable = model.device.connection {
                        Button("打开系统设置", action: openSystemSettings).frame(minHeight: 44)
                    }
                    if model.searchLocationStatus == .permissionDenied {
                        Button("前往设置开启定位", action: openSystemSettings).frame(minHeight: 44)
                    }
                    Text("车机保持开机并靠近 iPhone。连接成功后，当前导航会自动同步。")
                        .font(.caption).foregroundStyle(DucatiTheme.secondary)
                }.padding(20)
            }
            .ducatiScreen().navigationTitle("我的车机").navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("完成") { showsDeviceDetails = false }.accessibilityIdentifier("device-details-done")
                }
            }
            .accessibilityIdentifier("device-details-sheet")
        }
        .presentationDetents([.large]).presentationDragIndicator(.visible)
        .presentationBackground(DucatiTheme.background)
    }

    private func statusRow(_ title: String, symbol: String, value: String, color: Color) -> some View {
        LabeledContent {
            Text(value).foregroundStyle(color).multilineTextAlignment(.trailing)
        } label: {
            Label(title, systemImage: symbol).foregroundStyle(DucatiTheme.primary)
        }.font(.subheadline).padding(.vertical, 4)
    }

    private func failureMessage(_ message: String) -> some View {
        Label(message, systemImage: "exclamationmark.triangle")
            .font(.subheadline).foregroundStyle(DucatiTheme.warning)
            .fixedSize(horizontal: false, vertical: true).ducatiCard()
    }

    private func openSystemSettings() {
        guard let url = URL(string: UIApplication.openSettingsURLString) else { return }
        openURL(url)
    }

    // MARK: - User-facing state

    private func placeSubtitle(_ place: PlaceSearchResult) -> String {
        let area = place.displayArea.isEmpty
            ? [place.city, place.district].filter { !$0.isEmpty }.joined(separator: " · ")
            : place.displayArea
        var parts = [area, place.address].filter { !$0.isEmpty }
        if let distance = place.distanceM, distance >= 0 {
            let distanceText = distance >= 1_000
                ? String(format: "%.1f km", distance / 1_000)
                : "\(Int(distance.rounded())) m"
            parts.append("距你 \(distanceText)")
        }
        return parts.isEmpty ? "查看路线" : parts.joined(separator: " · ")
    }

    private var deviceStatus: String {
        switch model.device.connection {
        case .connected: return model.deviceReady ? "已连接" : "正在准备"
        case .scanning: return "正在寻找车机"
        case .connecting: return "正在连接"
        case .failed: return "连接未成功"
        case .bluetoothUnavailable: return "蓝牙不可用"
        case .idle: return "未连接"
        }
    }

    private var deviceName: String {
        switch model.device.connection {
        case let .connected(name), let .connecting(name): return name
        default: return "MOTO GPS"
        }
    }

    private var deviceDetail: String {
        switch model.device.connection {
        case .connected:
            return model.deviceReady ? "车机已就绪，可以接收导航指引。" : "正在准备导航同步，请稍候。"
        case .failed:
            return "暂时无法连接。请确认车机已开机并靠近手机，再试一次。"
        case .bluetoothUnavailable:
            return "请开启手机蓝牙，并允许 MOTO GPS 使用蓝牙。"
        case .scanning, .connecting:
            return "请将已开机的车机放在手机附近。"
        case .idle:
            return "连接你的车机，在车把上查看导航。"
        }
    }

    private var connectionActionTitle: String {
        switch model.device.connection {
        case .connected: return "断开连接"
        case .scanning: return "停止搜索"
        case .connecting: return "取消连接"
        case .failed: return "重新连接"
        case .idle, .bluetoothUnavailable: return "连接车机"
        }
    }

    private var searchLocationStatus: String {
        switch model.searchLocationStatus {
        case .available: return "已获取"
        case .preparing: return "获取中"
        case .permissionDenied: return "未获授权"
        case .unavailable: return "暂不可用"
        }
    }

    private var navigationTitle: String {
        if model.navigationFailure != nil { return "导航需要处理" }
        switch model.navigation.stateName {
        case "acquiring": return "正在获取位置"
        case "planning": return "正在规划路线"
        case "rerouting": return "正在重新规划"
        case "arrived": return "已到达目的地"
        case "navigating": return model.isDemoActive ? "正在演示" : "正在导航"
        default: return "准备出发"
        }
    }

    private var navigationDetail: String {
        if let failure = model.navigationFailure { return failure }
        if model.navigation.stateName == "navigating" {
            if model.isDemoActive { return "预览行驶过程与车机上的导航指引。" }
            return model.deviceReady
                ? "车机已连接，手机可锁屏收好。"
                : "车机连接后，导航指引会自动同步。"
        }
        return model.phaseDetail
    }

    private var navigationSymbol: String {
        if model.navigationFailure != nil { return "exclamationmark.triangle" }
        switch model.navigation.stateName {
        case "arrived": return "flag.checkered"
        case "rerouting": return "arrow.triangle.2.circlepath"
        case "navigating": return "location.north.fill"
        default: return "location.magnifyingglass"
        }
    }

    private var hasNavigationEstimate: Bool {
        ["navigating", "rerouting", "arrived"].contains(model.navigation.stateName)
    }

    private var remainingDuration: String {
        hasNavigationEstimate ? model.remainingDurationText : "—"
    }

    private var remainingDistance: String {
        hasNavigationEstimate ? model.remainingDistanceText : "—"
    }

    private var locationStatus: String {
        if model.isDemoActive { return "演示位置" }
        return model.navigation.hasUsableFix ? "已获取" : "获取中"
    }

    private var trafficStatus: String {
        if model.isDemoActive { return "演示中" }
        if model.navigation.trafficRequestInFlight { return "更新中" }
        return model.navigation.networkName == "online" ? "在线" : "离线"
    }

    private func trafficTint(_ candidate: RoutePreviewCandidate) -> Color {
        switch candidate.trafficSummary {
        case "拥堵较多": return DucatiTheme.accent
        case "部分路段缓行": return DucatiTheme.warning
        case "路况顺畅": return DucatiTheme.ready
        default: return DucatiTheme.secondary
        }
    }
}
