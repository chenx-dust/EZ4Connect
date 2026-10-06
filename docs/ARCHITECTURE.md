# 架构说明

EZ4Connect 使用轻量级分层架构。界面层通过 Coordinator 组合应用服务和平台实现，避免
`MainWindow` 直接承担连接进程、配置存储及认证窗口的生命周期。

## 目录职责

```text
core/             连接领域模型与状态机，不依赖界面和平台实现
application/      用例、会话及基础设施端口
infrastructure/   QProcess、QSettings、系统代理、文件和更新检查等实现
presentation/     Qt Widgets 界面、对话框和界面流程 Coordinator
tests/            按端口或纯逻辑边界测试各层行为
```

依赖方向为：

```text
presentation -> application -> core
       |              ^
       v              |
infrastructure -------+
```

`application` 不引用 `presentation` 或具体的 `infrastructure` 类型。具体实现统一由
`MainWindowCoordinator` 创建并注入：

- `CoreProcess` → `ZjuConnectProcess`
- `SystemProxyBackend` → `PlatformSystemProxyBackend`
- `ProfileBackend` → `ProfileManager`

## 主要流程

- `MainWindowCoordinator` 是主界面的组合入口，并维护 VPN 与系统代理之间的生命周期联动。
- `ConnectionUiController` 处理连接、断开、系统代理按钮及错误展示。
- `AuthDialogCoordinator` 管理登录、sudo、验证码、TOTP 和 SSO 对话框。
- `ConnectionSession` 负责核心进程、重连和连接状态，不依赖具体 `QProcess` 实现。
- `ProfileService` 持有当前配置上下文，`SettingsMigrator` 负责配置版本迁移。
- `CredentialStore` 是凭据读写端口，`KeychainCredentialStore` 通过 QtKeychain 访问各平台的系统凭据库；`MainWindowCoordinator` 将其注入认证界面。`SsoCredentialScope` 规范化配置和 HTTPS 来源，SSO 网页层只负责表单识别与输入事件，不直接调用系统钥匙串 API。

新增代码应按“变化原因”归位：界面行为放入 `presentation`，用例状态放入
`application`，平台或文件 I/O 放入 `infrastructure`，可独立验证的连接规则放入
`core`。不要重新引入通用 `utils` 目录；只有跨多个职责且没有明确归属的代码才需要新建
共享模块。
