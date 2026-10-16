import XCTest

final class GatewaySettingsUITests: XCTestCase {
    override func setUpWithError() throws { continueAfterFailure = false }

    func testSaveGatewayPersistsAcrossLaunches() {
        let app = XCUIApplication()
        app.launchArguments = ["--moto-ui-offline", "--moto-reset-gateway"]
        app.launch()
        let setup = app.buttons["gateway-setup-button"]
        XCTAssertTrue(setup.waitForExistence(timeout: 10))
        setup.tap()
        openOptionalGateway(app)
        let field = app.textFields["gateway-address-field"].exists
            ? app.textFields["gateway-address-field"] : app.textViews["gateway-address-field"]
        XCTAssertTrue(field.waitForExistence(timeout: 5))
        field.tap()
        field.typeText("https://nav.example.com/moto-gps/api")
        keepScreenshot(app, "Gateway address entry")
        app.buttons["gateway-save-button"].tap()
        app.buttons["取消"].tap()
        XCTAssertTrue(app.textFields["destination-search-field"].waitForExistence(timeout: 5))
        XCTAssertTrue(app.buttons["gateway-setup-button"].exists)
        app.terminate()
        app.launchArguments = ["--moto-ui-offline"]
        app.launch()
        let settings = app.buttons["gateway-settings-toolbar"]
        XCTAssertTrue(settings.waitForExistence(timeout: 5))
        settings.tap()
        openOptionalGateway(app)
        let saved = app.descendants(matching: .any).matching(identifier: "gateway-address-field").firstMatch
        XCTAssertTrue(saved.waitForExistence(timeout: 5))
        XCTAssertEqual(saved.value as? String, "https://nav.example.com/moto-gps/api/")
        keepScreenshot(app, "Saved gateway settings")
    }

    func testInvalidAddressStaysInSettingsAndExplainsError() {
        let app = XCUIApplication()
        app.launchArguments = ["--moto-ui-offline", "--moto-reset-gateway"]
        app.launch()
        let setup = app.buttons["gateway-setup-button"]
        XCTAssertTrue(setup.waitForExistence(timeout: 10))
        setup.tap()
        openOptionalGateway(app)
        let field = app.descendants(matching: .any).matching(identifier: "gateway-address-field").firstMatch
        XCTAssertTrue(field.waitForExistence(timeout: 5))
        field.tap()
        field.typeText("http://nav.example.com")
        app.buttons["gateway-save-button"].tap()
        let error = app.staticTexts["gateway-status-message"]
        XCTAssertTrue(error.waitForExistence(timeout: 5))
        XCTAssertTrue(error.label.contains("HTTPS"))
        keepScreenshot(app, "Invalid gateway address")
    }

    func testSDKConsentIsRequiredBeforeSaving() {
        let app = XCUIApplication()
        app.launchArguments = ["--moto-ui-offline"]
        app.launch()
        let settings = app.buttons["gateway-settings-toolbar"]
        XCTAssertTrue(settings.waitForExistence(timeout: 10))
        settings.tap()
        XCTAssertTrue(app.secureTextFields["amap-ios-key"].waitForExistence(timeout: 5))
        XCTAssertTrue(app.switches["amap-sdk-consent"].exists)
        XCTAssertFalse(app.buttons["保存"].isEnabled)
        keepScreenshot(app, "Native SDK privacy and key setup")
    }

    private func openOptionalGateway(_ app: XCUIApplication) {
        XCTAssertTrue(app.secureTextFields["amap-ios-key"].waitForExistence(timeout: 5))
        let button = app.buttons["配置可选底图网关"]
        for _ in 0 ..< 4 {
            if button.exists && button.isHittable { button.tap(); return }
            app.swipeUp()
        }
        XCTFail("Optional map gateway entry is not reachable")
    }

    private func keepScreenshot(_ app: XCUIApplication, _ name: String) {
        let attachment = XCTAttachment(screenshot: app.screenshot())
        attachment.name = name
        attachment.lifetime = .keepAlways
        add(attachment)
    }
}
