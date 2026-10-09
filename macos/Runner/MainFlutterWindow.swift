import Cocoa
import FlutterMacOS
import window_manager
import LaunchAtLogin

class MainFlutterWindow: NSWindow {
    private var capturedBrowser: NSRunningApplication?
    private let chromiumBrowsers: Set<String> = [
        "com.google.Chrome", "com.google.Chrome.beta", "com.google.Chrome.canary",
        "com.microsoft.edgemac", "com.brave.Browser", "company.thebrowser.Browser",
        "org.chromium.Chromium"
    ]

    override func awakeFromNib() {
        let flutterViewController = FlutterViewController()
        let windowFrame = self.frame
        self.contentViewController = flutterViewController
        self.setFrame(windowFrame, display: true)

        setupTitlebar()

        FlutterMethodChannel(
            name: "betterbox/current_website",
            binaryMessenger: flutterViewController.engine.binaryMessenger
        ).setMethodCallHandler { [weak self] call, result in
            guard let self = self else { return result(nil) }
            switch call.method {
            case "captureBrowser":
                self.capturedBrowser = NSWorkspace.shared.frontmostApplication
                result(nil)
            case "readURL":
                self.readCurrentWebsite(result: result)
            default:
                result(FlutterMethodNotImplemented)
            }
        }

        FlutterMethodChannel(
            name: "launch_at_startup", binaryMessenger: flutterViewController.engine.binaryMessenger
        )
        .setMethodCallHandler { (_ call: FlutterMethodCall, result: @escaping FlutterResult) in
            switch call.method {
            case "launchAtStartupIsEnabled":
                result(LaunchAtLogin.isEnabled)
            case "launchAtStartupSetEnabled":
                if let arguments = call.arguments as? [String: Any] {
                    LaunchAtLogin.isEnabled = arguments["setEnabledValue"] as! Bool
                }
                result(nil)
            default:
                result(FlutterMethodNotImplemented)
            }
        }

        RegisterGeneratedPlugins(registry: flutterViewController)

        super.awakeFromNib()
    }

    private func setupTitlebar() {
        self.titleVisibility = .hidden
        self.titlebarAppearsTransparent = true
        self.styleMask.insert(.fullSizeContentView)

        let toolbar = NSToolbar(identifier: "MainAppToolbar")
        self.toolbar = toolbar
        if #available(macOS 11.0, *) {
            self.toolbarStyle = .unifiedCompact
            self.titlebarSeparatorStyle = .none
        }
    }

    private func readCurrentWebsite(result: @escaping FlutterResult) {
        guard let browser = capturedBrowser,
              !browser.isTerminated,
              let identifier = browser.bundleIdentifier,
              identifier == "com.apple.Safari" || chromiumBrowsers.contains(identifier) else {
            result(FlutterError(code: "unsupported_browser", message: nil, details: nil))
            return
        }
        capturedBrowser = nil
        let tabExpression = identifier == "com.apple.Safari"
            ? "URL of current tab of front window"
            : "URL of active tab of front window"
        // Only fixed, allowlisted application identifiers enter AppleScript.
        let source = """
        with timeout of 5 seconds
            tell application id "\(identifier)"
                if (count of windows) is 0 then return ""
                return \(tabExpression)
            end tell
        end timeout
        """
        // NSAppleScript must execute on the main thread; the script is bounded.
        var error: NSDictionary?
        let value = NSAppleScript(source: source)?.executeAndReturnError(&error).stringValue
        let errorCode = (error?[NSAppleScript.errorNumber] as? NSNumber)?.intValue
        if error != nil {
            result(FlutterError(
                code: errorCode == -1743 ? "automation_denied" : "browser_read_failed",
                message: nil, details: nil
            ))
        } else if let value = value, !value.isEmpty {
            result(value)
        } else {
            result(FlutterError(code: "no_webpage", message: nil, details: nil))
        }
    }

    override public func order(_ place: NSWindow.OrderingMode, relativeTo otherWin: Int) {
        super.order(place, relativeTo: otherWin)
        hiddenWindowAtLaunch()
    }
}
