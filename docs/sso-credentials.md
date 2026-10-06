# SSO 凭据保存

SSO 登录窗口提供“保存账号密码”“填充已保存账号”和“删除本页已保存账号”。

第一次使用时，在 SSO 网页的账号登录表单输入账号和密码，点击“保存账号密码”，再按网页原有流程登录。下次打开相同配置的相同 HTTPS 登录网站，程序会自动填充保存的账号密码。登录按钮、验证码和多因素认证仍由用户操作；修改密码后可重新输入并保存。

## 系统凭据库

凭据通过 [QtKeychain](https://github.com/frankosterfeld/qtkeychain) 存入当前操作系统的安全存储：

| 平台 | 存储方式 |
| --- | --- |
| Windows | Windows 凭据管理器 |
| macOS | macOS 钥匙串 |
| Linux | Secret Service（如 GNOME Keyring）或 KWallet |

Linux 需要可用的桌面会话和已安装、可解锁的凭据服务。系统拒绝访问、凭据库锁定或服务不可用时，界面提示失败，仍可手动登录。程序明确禁用 QtKeychain 的不安全回退，不把 SSO 密码写入配置文件、命令行或日志。保存失败时不会降级为文件保存。

## 保存范围与表单识别

保存范围包含当前配置文件路径、VPN 服务来源，以及 SSO 页面的 HTTPS 主机和端口；网页路径和临时 OAuth 参数不参与标识。不同配置文件、VPN 服务器或网站不共用密码。配置重命名、移动或复制后需要重新保存；可在旧配置的 SSO 页删除旧条目，或在系统凭据管理工具中管理 EZ4Connect 的条目。

自动填充只识别主页面可见、可编辑、且只有一个密码框的账号登录表单。已有其他账号、修改密码表单、验证码表单、子框架和跨网站提交表单会被跳过；网页结构不能识别时仍可手动登录。程序不会自动点击登录按钮，也不会绕过 TLS 验证或验证码。

本功能针对 SSO 网页登录。普通 VPN、证书密码和 TOTP 的配置存储尚未迁移，因此路线图中的完整凭据迁移仍未完成。系统可能在首次保存、读取或更换应用签名后请求解锁或访问授权。

## 开发验证

- `ssocredentialscope_test` 检查 HTTPS 来源规范化与配置隔离。
- `sso_credentials_script_test` 在实际 Qt WebEngine 中验证合成网页的填充、输入事件和保护条件。
- `ssologinwebview_test` 使用模拟凭据端口检查界面保存、填充、删除、拒绝访问和窗口关闭期间的回调。
- `keychaincredentialstore_test` 使用随机独立条目验证真实系统凭据库保存、读取、替换、删除与无效数据处理，并清理测试条目。无凭据服务的开发环境可跳过；设置 `EZ4CONNECT_REQUIRE_CREDENTIAL_STORE_TEST=1` 时必须通过。

测试不使用真实账号密码。Linux WebEngine 测试需要显示服务，可通过 `xvfb-run -a ctest --test-dir build --output-on-failure` 运行。CI 为 Linux 凭据测试启动临时 D-Bus 与 GNOME Keyring 会话。
