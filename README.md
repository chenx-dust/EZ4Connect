# EZ4Connect

*前身为 HITsz Connect for Windows*

![Action](https://github.com/chenx-dust/EZ4Connect/actions/workflows/build.yml/badge.svg)
![Release](https://img.shields.io/github/v/release/chenx-dust/EZ4Connect)
![Downloads](https://img.shields.io/github/downloads/chenx-dust/EZ4Connect/total)
![License](https://img.shields.io/github/license/chenx-dust/EZ4Connect)

改进的 ZJU-Connect 图形界面

## 🎉 现已正式提供 aTrust 支持

如使用中遇到问题，可加入 ZJU-Connect 用户反馈 QQ 群 1037726410 交流。

<div align="center">
<img src="docs/main.png" width="600px">
</div>

## 使用方式

在本项目的 [Releases](https://github.com/chenx-dust/EZ4Connect/releases) 页面下载最新版本：

- **Windows 用户**：下载 `EZ4Connect-vX.X.X-windows-ARCH.zip` ，解压至同一目录下，双击运行 `EZ4Connect.exe` ；
  - 如果遇到缺少 DLL 等问题，请先下载安装 Microsoft Visual C++ 可再发行程序包版本（[x64](https://aka.ms/vs/17/release/vc_redist.x64.exe) | [arm64](https://aka.ms/vs/17/release/vc_redist.arm64.exe)），再运行程序；
- **macOS 用户**：下载 `EZ4Connect-vX.X.X-macOS-ARCH.dmg` ，将 EZ4Connect 移动到应用程序目录中；
  - ~~如果遇到“Apple 无法检查 App 是否包含恶意软件”等报错，请参考 [Apple 支持](https://support.apple.com/zh-cn/guide/mac-help/mchleab3a043/mac) 进行操作。~~本软件已通过 Apple 官方公证，可直接运行；
- **Linux 用户**：下载 `EZ4Connect-vX.X.X-linux-ARCH.AppImage` ，赋予执行权限，运行即可；
  - AppImage x64 仅支持系统 `glibc >= 2.31` 的发行版，Ubuntu 22.04 及以上版本可以正常运行（受限于 GitHub Actions Runner）；
  - AppImage arm64 仅支持系统 `glibc >= 2.38` 的发行版，Ubuntu 24.04 及以上版本可以正常运行（受限于 Qt 官方：[参考](https://doc.qt.io/qt-6/supported-platforms.html)）；
  - Arch Linux 用户推荐使用 [AUR](https://aur.archlinux.org/packages/ez4connect) 安装；
  - 如果遇到因依赖问题无法运行的情况，请自行编译运行。

1. 跟随配置向导进行服务器与账户设置，或在“设置”中手动配置；

2. 在主界面中点击“连接服务器”。如果只需进行校园网页浏览，则选择“设置系统代理”后即可使用。

如果需要配合 Clash / Mihomo 进行高级的分流操作，可以参见： [高级使用方式](docs/ADVANCED_USAGE.md)

## 退出路径（SIGTERM/SIGINT）

注销会话、`systemctl --user stop`，或者直接用 `kill` 结束界面时发的是 SIGTERM。此前进程就地消失：核心子进程被遗留（隧道还在跑、端口还被占着），系统代理也不会被清除。现在这个信号会转成一次普通退出请求，因此走的是与托盘退出完全相同的流程——清除系统代理，再由对象析构结束核心子进程。

已在 Linux 上手工验证：用替身核心跑 `kill -TERM` 后界面与核心子进程都不再存在（日志末行为「收到终止信号，正在退出」），且该次退出的日志里随后执行了清除系统代理的命令。本改动只覆盖 SIGTERM/SIGINT 这条可捕获的信号路径；未覆盖、也未验证：信号不可捕获的死法（崩溃等），以及退出时界面上存在模态对话框的情形。

## 路线图

如有更多好的建议，可以在 Issue 中或是 OSA 群里提出！

- [X] 支持 macOS 系统
- [X] 支持 Linux 系统
- [X] 支持手动设置 Proxy Bypass
- [X] 上传 AUR 包
- [ ] 使用密钥链存储密码等信息

## 开发

项目的分层、依赖方向和新增代码归属规则见
[架构说明](docs/ARCHITECTURE.md)。

## 许可证

本项目遵循 [GNU General Public License Version 3](LICENSE) 开源。

## 致谢

- [Mythologyli/ZJU-Connect-for-Windows](https://github.com/Mythologyli/ZJU-Connect-for-Windows)
- [Mythologyli/zju-connect](https://github.com/Mythologyli/zju-connect)

> 欢迎加入 HITSZ 开源技术协会 [@hitszosa](https://github.com/hitszosa)
