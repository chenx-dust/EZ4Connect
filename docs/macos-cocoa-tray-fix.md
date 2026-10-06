# macOS Cocoa 托盘兼容修复

macOS 27 的状态栏按钮回调可能收到非鼠标事件。Qt 6.10.3 原有 Cocoa 托盘实现直接读取该事件的 `clickCount`，会触发 Objective-C 异常并退出。这里回补 [Qt 上游修复](https://github.com/qt/qtbase/commit/6192d9edd00caa14ed6b67c32c0cd8cfe95cf815)：非鼠标事件返回 `Unknown` 激活原因，鼠标单击、双击、右键和中键仍保持原有行为。

## 构建与部署

`scripts/build-macos-cocoa-tray-fix.sh <app_build_dir> <arm64|x86_64>` 从经过 SHA-256 校验的 [QtBase v6.10.3 源码](https://github.com/qt/qtbase/tree/v6.10.3) 构建 Cocoa 平台插件和独立原生回归探针，要求 CMake 3.21 及以上。它使用应用构建时的 Qt SDK，要求同版 Framework、Private headers、导出特性和目标架构；版本或配置无法验证时直接失败。离线构建可通过 `--source-archive` 指定同一校验过的源码归档。

结果位于 `<app_build_dir>/macos-cocoa-tray-fix`。插件只使用包内相对 Framework 搜索路径 `@loader_path/../../Frameworks`。脚本不会修改 Qt SDK，也不把预编译插件作为源码提交。

`scripts/deploy-macos.sh` 先执行 `macdeployqt` 和架构裁剪，再构建并替换 Cocoa 插件。它为整个应用重新生成 ad-hoc 签名并严格验证，随后针对包内插件和 Frameworks 运行原生探针；任何步骤失败都会中止打包。Release 的 Developer ID 签名、hardened runtime 和 Apple 公证在此之后按原有流程执行。

应用内 `Contents/Resources/qt-cocoa-tray-fix` 保存源码 URL、SHA-256、配置清单、实际补丁和相关许可证。这里提供精确的源码获取与重建信息，完整 QtBase 归档可按清单地址下载校验，不随应用重复打包。

## 回归验证

`tests/macos_tray_event_test.mm` 不连接 VPN，也不请求网站。它为两个真实 Cocoa 回调分别构造 AppKit 事件、应用事件、键盘事件、空事件、鼠标单击、双击、右键和中键，共 16 项检查，验证激活信号次数、原因及 Objective-C 异常。

探针要求实际加载 Qt 6.10.3，并用 `dladdr` 验证 Cocoa 回调实现和 QtCore/QtGui 来自指定目录，避免使用 SDK 插件代替包内插件而误判通过。CI 对 ARM64 和 x64 都检查源码构建插件及最终部署插件；Apple Silicon 上运行 x64 探针需要 Rosetta。

```sh
qt_prefix="$(qmake -query QT_INSTALL_PREFIX)"
TRAY_TEST_PLUGIN_ROOT="$PWD/build/macos-cocoa-tray-fix" \
TRAY_TEST_FRAMEWORK_ROOT="$qt_prefix/lib" \
DYLD_FRAMEWORK_PATH="$qt_prefix/lib" \
  build/macos-cocoa-tray-fix/macos_tray_event_test
```

对最终应用测试时，把插件目录改为 `EZ4Connect.app/Contents/PlugIns`，两个 Framework 变量改为该应用的 `Contents/Frameworks` 绝对路径。

## 后续移除条件

保留 Qt 6.10.3 是为了避免仅为此修复抬高支持的系统下限：[Qt 6.10 支持 macOS 13 及以上](https://doc.qt.io/qt-6.10/supported-platforms.html)，而已包含修复的 [Qt 6.12 最低要求 macOS 14.4](https://doc.qt.io/qt-6.12/supported-platforms.html)。以后接受新的系统下限并将整个 macOS Qt SDK、Frameworks 和插件升级到已经修复的官方版本后，可删除此回补。不能只替换跨 Qt 版本的 Cocoa 插件。
