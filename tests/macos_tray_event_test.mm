#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <objc/runtime.h>

#include <QtGui/QGuiApplication>
#include <QtGui/private/qguiapplication_p.h>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <qpa/qplatformtheme.h>
#include <qpa/qplatformsystemtrayicon.h>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>

static NSEvent *testCurrentEvent = nil;
static bool overrideCurrentEvent = false;

@interface TrayRegressionApplication : NSApplication
@end
@implementation TrayRegressionApplication
- (NSEvent *)currentEvent
{
    return overrideCurrentEvent ? testCurrentEvent : [super currentEvent];
}
@end

@interface NSObject (TrayRegressionDelegate)
- (instancetype)initWithSysTray:(QPlatformSystemTrayIcon *)tray;
- (void)statusItemClicked;
- (void)statusItemMenuBeganTracking:(NSNotification *)notification;
@end

int main(int argc, char **argv)
{
    @autoreleasepool {
        // The probe exercises Qt private ABI and must use the patched release.
        if (std::strcmp(qVersion(), "6.10.3") != 0) {
            std::fprintf(stderr, "FAIL: tray probe requires Qt 6.10.3, got %s\n", qVersion());
            return 2;
        }
        const QString pluginRoot = qEnvironmentVariable("TRAY_TEST_PLUGIN_ROOT");
        const QString frameworkRoot = qEnvironmentVariable("TRAY_TEST_FRAMEWORK_ROOT");
        const QString expectedPlugin = QFileInfo(QDir(pluginRoot).filePath(
            "platforms/libqcocoa.dylib")).canonicalFilePath();
        if (pluginRoot.isEmpty() || frameworkRoot.isEmpty() || expectedPlugin.isEmpty()) {
            std::fprintf(stderr, "FAIL: set tray plugin and framework roots to existing build or bundle paths\n");
            return 2;
        }
        [TrayRegressionApplication sharedApplication];
        QCoreApplication::setLibraryPaths({pluginRoot});
        QGuiApplication app(argc, argv);
        Dl_info coreLibrary{}, guiLibrary{};
        dladdr(reinterpret_cast<const void *>(&qVersion), &coreLibrary);
        dladdr(reinterpret_cast<const void *>(&QGuiApplication::platformName), &guiLibrary);
        const auto matchesLibrary = [](const Dl_info &library, const QString &expected) {
            return library.dli_fname && !expected.isEmpty()
                && QFileInfo(QString::fromUtf8(library.dli_fname)).canonicalFilePath() == expected;
        };
        if (!matchesLibrary(coreLibrary, QFileInfo(QDir(frameworkRoot).filePath(
                "QtCore.framework/Versions/A/QtCore")).canonicalFilePath())
            || !matchesLibrary(guiLibrary, QFileInfo(QDir(frameworkRoot).filePath(
                "QtGui.framework/Versions/A/QtGui")).canonicalFilePath())) {
            std::fprintf(stderr, "FAIL: tray probe loaded Qt frameworks outside the requested runtime\n");
            return 2;
        }
        std::printf("QtCore: %s\nQtGui: %s\n", coreLibrary.dli_fname, guiLibrary.dli_fname);
        auto *tray = QGuiApplicationPrivate::platformTheme()->createPlatformSystemTrayIcon();
        if (!tray) {
            std::fprintf(stderr, "FAIL: no native tray implementation\n");
            return 2;
        }
        Class delegateClass = objc_getClass("QStatusItemDelegate");
        if (!delegateClass) {
            std::fprintf(stderr, "FAIL: no Cocoa tray delegate\n");
            delete tray;
            return 2;
        }
        const Method callback = class_getInstanceMethod(delegateClass, @selector(statusItemClicked));
        Dl_info cocoaLibrary{};
        if (!callback || !dladdr(reinterpret_cast<const void *>(method_getImplementation(callback)),
                                &cocoaLibrary)
            || !matchesLibrary(cocoaLibrary, expectedPlugin)) {
            std::fprintf(stderr, "FAIL: tray delegate did not come from the requested Cocoa plugin\n");
            delete tray;
            return 2;
        }
        std::printf("Cocoa plugin: %s\n", cocoaLibrary.dli_fname);
        id delegate = [[delegateClass alloc] initWithSysTray:tray];
        int signalCount = 0;
        auto reason = QPlatformSystemTrayIcon::Unknown;
        QObject::connect(tray, &QPlatformSystemTrayIcon::activated, &app,
                         [&](QPlatformSystemTrayIcon::ActivationReason actual) {
            ++signalCount;
            reason = actual;
        });
        overrideCurrentEvent = true;
        int tested = 0;
        bool passed = true;
        auto check = [&](NSEvent *event, QPlatformSystemTrayIcon::ActivationReason expected,
                         const char *name) {
            testCurrentEvent = event;
            for (int callback = 0; callback < 2; ++callback) {
                const int before = signalCount;
                @try {
                    if (callback == 0)
                        [delegate statusItemClicked];
                    else
                        [delegate statusItemMenuBeganTracking:nil];
                    if (signalCount != before + 1 || reason != expected) {
                        std::fprintf(stderr, "FAIL: %s callback=%d reason=%d signals=%d\n",
                                     name, callback, int(reason), signalCount - before);
                        passed = false;
                    }
                } @catch (NSException *exception) {
                    std::fprintf(stderr, "FAIL: %s callback=%d exception=%s\n", name, callback,
                                 exception.name.UTF8String);
                    passed = false;
                }
                ++tested;
            }
        };
        check([NSEvent otherEventWithType:NSEventTypeAppKitDefined location:NSZeroPoint
            modifierFlags:0 timestamp:0 windowNumber:0 context:nil subtype:255 data1:0 data2:0],
            QPlatformSystemTrayIcon::Unknown, "AppKitDefined");
        check([NSEvent otherEventWithType:NSEventTypeApplicationDefined location:NSZeroPoint
            modifierFlags:0 timestamp:0 windowNumber:0 context:nil subtype:0 data1:0 data2:0],
            QPlatformSystemTrayIcon::Unknown, "ApplicationDefined");
        check([NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:0
            timestamp:0 windowNumber:0 context:nil characters:@"a" charactersIgnoringModifiers:@"a"
            isARepeat:NO keyCode:0], QPlatformSystemTrayIcon::Unknown, "KeyDown");
        check(nil, QPlatformSystemTrayIcon::Unknown, "nil");
        check([NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:NSZeroPoint
            modifierFlags:0 timestamp:0 windowNumber:0 context:nil eventNumber:0 clickCount:1 pressure:0],
            QPlatformSystemTrayIcon::Trigger, "left");
        check([NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:NSZeroPoint
            modifierFlags:0 timestamp:0 windowNumber:0 context:nil eventNumber:0 clickCount:2 pressure:0],
            QPlatformSystemTrayIcon::DoubleClick, "double");
        check([NSEvent mouseEventWithType:NSEventTypeRightMouseDown location:NSZeroPoint
            modifierFlags:0 timestamp:0 windowNumber:0 context:nil eventNumber:0 clickCount:1 pressure:0],
            QPlatformSystemTrayIcon::Context, "right");
        // NSEvent's convenience constructor leaves otherMouseDown.buttonNumber
        // at zero. A synthetic CGEvent preserves the actual center-button value.
        CGEventRef middleEvent = CGEventCreateMouseEvent(nullptr, kCGEventOtherMouseDown,
            CGPointZero, kCGMouseButtonCenter);
        if (middleEvent) {
            CGEventSetIntegerValueField(middleEvent, kCGMouseEventClickState, 1);
            check([NSEvent eventWithCGEvent:middleEvent], QPlatformSystemTrayIcon::MiddleClick, "middle");
            CFRelease(middleEvent);
        } else {
            std::fprintf(stderr, "FAIL: could not construct the synthetic center-button event\n");
            passed = false;
        }
        overrideCurrentEvent = false;
        [delegate release];
        delete tray;
        std::printf("%s: %d native tray event cases\n", passed ? "PASS" : "FAIL", tested);
        return passed ? 0 : 1;
    }
}
