# 高级使用方式

## Clash / Mihomo 分流配置

若需要使用 Clash、Mihomo 等其他代理软件与本软件配合分流，可以采用以下两种方式之一：

1. **本软件作为主代理**：开启本软件的系统代理功能，将本软件认为不需要代理的流量通过“直连代理”转发给 Clash 或 Mihomo。
2. **Clash/Mihomo 作为主代理**：关闭本软件的系统代理功能，将需要本软件代理的流量转发到本软件。

以下是具体操作流程：

### 方式一：本软件作为主代理

- 在“功能”-“设置”-“核心” 页面中设置“直连代理”为 Clash 等软件监听的端口（以 7890 为例 http://127.0.0.1:7890）；
- 注意勾选“允许外部访问”；
- 开启本软件的系统代理；
- 启动 Clash 时不需要打开系统代理或 TUN，即可正确转发流量。

<div align="center">
<img src="proxy_setting.png" width="600px">
</div>

### 方式二：Clash 作为主代理

- 清空本软件的系统代理；
- 在 “功能”-“设置”-“核心” 页面中设置 SOCK5 的代理端口（以 11080 为例），以下为推荐配置：

在 Clash 的代理配置中添加一个代理服务器：

```yaml
# 代理服务器
proxies:
  - name: 🖥 EZ4Connect
    type: socks5
    server: 127.0.0.1
    port: 11080
    udp: true
```

并在代理组中添加一个单独的代理组：

```yaml
proxy-groups:
  - name: "🏫 校园网"
    type: select
    proxies:
      - DIRECT
      - 🖥 EZ4Connect
```

并在规则中加入：

```yaml
rules:
  - DOMAIN,vpn.hitsz.edu.cn,DIRECT
  - DOMAIN-SUFFIX,hitsz.edu.cn,🏫 校园网
  - IP-CIDR,10.0.0.0/8,🏫 校园网,no-resolve
  # 可在此添加其它你需要代理的 ip 段，如课程中心
```

这样即可通过简单的切换实现在校外使用本项目时选择 EZ4Connect 代理，在校内使用 DIRECT 直连。

<div align="center">
<img src="proxy_group.png" width="600px">
</div>

## TUN 模式

（这里是个坑，没填完的坑）

### Clash 作为主代理

在本方式中，Clash 提供 TUN 虚拟网卡服务，捕获全部流量并将预筛选符合条件的流量发送给 EZ4Connect 代理。

EZ4Connect 收到的流量会经过内部分流，将软件需要代理流量（默认为校园网流量）送入 VPN 通道，其余流量直接放行。

**这里需要特别注意**，在 Clash TUN 网卡的作用下，EZ4Connect 送出的流量会再次回到 Clash，因此本方式中**务必设置规则以排除这部分流量**，防止其再次被代理送回 EZ4Connect 引起回环。

1. 取消/清空本软件的系统代理，无需设置“直连代理”，注意勾选“允许外部访问”；
2. 在 Clash 中配置 TUN 相关设置。

推荐配置方式如下：

在 Clash 的代理配置中添加一个代理服务器：

```yaml
# 代理服务器
proxies:
  - name: 🖥 EZ4Connect
    type: socks5
    server: 127.0.0.1
    port: 11080
    udp: true
```

并在代理组中添加一个单独的代理组：

```yaml
proxy-groups:
  - name: "🏫 校园网"
    type: select
    proxies:
      - DIRECT
      - 🖥 EZ4Connect
```

并在规则中加入：

```yaml
rules:
  - DOMAIN,vpn.hitsz.edu.cn,DIRECT
  - PROCESS-PATH-WILDCARD,*EZ4Connect*,DIRECT
  - PROCESS-NAME,zju-connect.exe,DIRECT
  - PROCESS-NAME,EZ4Connect.exe,DIRECT
  - DOMAIN-SUFFIX,hitsz.edu.cn,🏫 校园网
  - IP-CIDR,10.0.0.0/8,🏫 校园网,no-resolve
  # 可在此添加其它你需要代理的 ip 段，如课程中心
```

其中：
- `PROCESS-PATH-WILDCARD`匹配路径中包含 EZ4Connect 的所有进程流量（主要用于匹配整个安装路径，如果安装文件夹名不同，可根据自身情况修改）；
- `PROCESS-NAME`精确匹配`EZ4Connect.exe`和`zju-connect.exe`联网核心进程；
- 上述两类规则在可以正确匹配的情况下选其一或保留两者均可，推荐使用`PROCESS-NAME`。并且**必须**至少放在`DOMAIN-SUFFIX,hitsz.edu.cn,🏫 校园网`之前，以达到放行流量，防止回环的目的。

最后，还需要在 DNS 配置中为`fake-ip`添加过滤规则，防止 EZ4Connect 的域名解析到 fake-ip 地址，从而无法正确分流。

```yaml
dns:
  # 仅在使用 fake-ip 时需要配置
  enhanced-mode: fake-ip
  fake-ip-filter:
    - +.hitsz.edu.cn
```

如果使用 Clash 的“全局扩展脚本”功能动态修改配置，以下是最小示例，可自行对比修改：

```javascript
// Define main function (script entry)

// DNS 配置
const dnsConfig = {
	"enhanced-mode": "fake-ip",
	"fake-ip-filter": ["+.hitsz.edu.cn"],
};

function main(config, profileName) {
	// 使用先前定义的 DNS 设置（会覆盖整个 dns 对象）
	// 如果希望增量修改，请自行重写
	config["dns"] = dnsConfig;

	// 校园网
	config.proxies = config.proxies || [];
	config["proxy-groups"] = config["proxy-groups"] || [];
	config.rules = config.rules || [];

	config.proxies.push({
		name: "EZ4Connect",
		type: "socks5",
		server: "127.0.0.1",
		port: 11080,
		udp: true,
	});

	config["proxy-groups"].push({
		name: "校园网",
		type: "select",
		proxies: ["DIRECT", "EZ4Connect"],
	});

	config.rules.unshift(
		"DOMAIN,vpn.hitsz.edu.cn,DIRECT",
		"PROCESS-PATH-WILDCARD,*EZ4Connect*,DIRECT",
		"PROCESS-NAME,zju-connect.exe,DIRECT",
		"PROCESS-NAME,EZ4Connect.exe,DIRECT",
		"DOMAIN-SUFFIX,hitsz.edu.cn, 校园网",
		"IP-CIDR,10.0.0.0/8, 校园网,no-resolve",
	);

	// 返回修改好的配置
	return config;
}
```

额外注意，`tun`的`route-exclude-address`（默认为空）不应加入`10.0.0.0/8`网段，否则会导致流量不能正确经由 tun 转发到 EZ4Connect。
