# Ubuntu TUN / DNS / Docker 问题的技术原理与调试说明

更新时间：2026-03-13

## 1. 这次问题的最终结论

这次故障不是一个单点问题，而是两个独立问题叠加：

1. `systemd-resolved` 在 `pypi.org` 上遇到了 no-EDNS 异常回包，并把它判定为 `Received invalid reply`。
2. `v2rayN` 的 `singbox_tun` 使用 `172.18.0.1/30`，而本机 Docker 里的 Dify 默认网络占用了 `172.18.0.0/16`，并且容器里还实际用了 `172.18.0.2`。

第一个问题解释了为什么只有一部分域名会坏，尤其是 `pypi.org` / `files.pythonhosted.org` 这条链路看起来最明显。第二个问题解释了为什么同一台机器上的网络状态会显得“混乱、时好时坏、很难直观看懂”。

最终修复不是简单“换 DNS”：

- 主机侧不再经由 `systemd-resolved` 直接处理应用 DNS 请求。
- 主机应用改为访问 `127.0.0.1` 上的本地 `dnsmasq`。
- 这个 `dnsmasq` 不直连公网 DNS，而是只把请求转发到 `172.18.0.2`，也就是 sing-box TUN DNS。
- Dify 的 Docker 部署和对应网络被移除，因此 `172.18.0.0/16` 冲突消失。

这意味着修复后同时满足两个目标：

- `pypi.org` 能恢复正常解析与下载。
- DNS 仍然走 sing-box / TUN / 代理链路，不发生“为了能用而把 DNS 直连公网”的泄露回退。

## 2. 故障前的真实解析链路

修复前，主机上的典型 DNS 链路是：

```text
应用
  -> glibc / getaddrinfo
  -> /etc/resolv.conf
  -> 127.0.0.53 (systemd-resolved stub)
  -> link: singbox_tun
  -> 172.18.0.2 (sing-box TUN DNS)
  -> sing-box 的上游 DNS 服务器
  -> 远端 DNS / DoH
```

从 `resolvectl status` 可以确认几个关键事实：

- `/etc/resolv.conf` 指向 `127.0.0.53`
- `singbox_tun` 被标记为 `DNS Domain: ~.`
- `singbox_tun` 的 DNS server 是 `172.18.0.2`

`~.` 的含义是“该链路可以处理所有域名”。因此只要 TUN 在运行，主机几乎所有普通 DNS 查询都会经过 `systemd-resolved -> singbox_tun -> 172.18.0.2`。

这条设计本身并不必然错误，但它有一个前提：

- `systemd-resolved` 必须能稳定接受上游返回的 DNS 报文。

而这次坏掉的地方，正是这一层。

## 3. 为什么 `pypi.org` 会报 `Received invalid reply`

### 3.1 关键现象

现场验证出现了一个看似矛盾、但实际上非常关键的组合：

- `resolvectl query pypi.org` 失败，报 `Received invalid reply`
- `dig @172.18.0.2 pypi.org A` 成功，返回 `NOERROR`
- `dig +noedns @172.18.0.2 pypi.org A` 成功，但回包里带异常 `OPT`
- `dig +noedns @192.168.110.1 pypi.org A` 也能复现同样异常

这组现象说明：

- 不是 `172.18.0.2` 完全不可用
- 不是 sing-box 本身彻底坏掉
- 也不是 “PyPI 服务器完全不可达”
- 真正的问题是：某些 no-EDNS 查询回来的 DNS 报文格式，对 `systemd-resolved` 来说不合法

### 3.2 为什么 direct `dig` 可以成功，`resolvectl` 却失败

这两者不是同一个客户端：

- `dig` 是一个通用 DNS 调试客户端，容错和展示方式与系统 resolver 不同
- `systemd-resolved` 是系统级 resolver，它会对收到的 DNS 报文做更严格的合法性检查

所以这次实际上是：

1. sing-box / 上游返回了一个“`dig` 还能接受、但 `systemd-resolved` 不接受”的报文
2. `systemd-resolved` 直接拒收
3. 应用层看到的就不是“部分答案不优雅”，而是“整个解析失败”

也就是说，`resolvectl query pypi.org` 的失败，并不等价于 “上游没有回答”，而是 “上游回答了，但被 systemd-resolved 视为非法”

### 3.3 为什么偏偏是 `pypi.org`

这次观测里，`python.org` 和其他普通域名可以正常通过同一路径解析，而 `pypi.org` 更容易触发异常。这说明问题是域名特异的，不是整条 DNS 链路对所有域名都坏。

直观理解可以是：

- 某些域名在某种查询方式下，权威或递归链路会返回带异常 `OPT` 的报文
- `systemd-resolved` 正好对这种情况很严格
- 因此这些域名成为“症状最明显”的受害者

这也是为什么你会感觉“不是全网都坏，但 `poetry` / `pip` / `PyPI` 特别容易出事”。

## 4. `systemd-resolved` 在这里为什么成了问题放大器

### 4.1 它不是上游，但它是决策点

`systemd-resolved` 并不生成远端答案，它只是：

- 接收应用查询
- 选择链路
- 接收上游报文
- 决定接受还是拒绝

一旦它拒收，上层应用看到的就是解析失败。

### 4.2 它还会做 feature downgrade

日志里能看到类似：

```text
Using degraded feature set UDP instead of UDP+EDNS0
```

这说明 `systemd-resolved` 在与上游交互时会根据历史行为和兼容性自动降级能力集。对大多数网络来说这是“容错”；但在这次场景里，它反而增加了命中 no-EDNS 异常回包路径的概率。

也就是说，这次不是简单的“服务器回错了一个包”，而是：

- `systemd-resolved` 会主动调整查询特征
- 某种查询特征正好触发了 `pypi.org` 的异常返回
- 最终整个主机 DNS 被放大为失败

### 4.3 为什么不能继续把它放在活跃路径里

如果你仍然让应用统一走：

```text
应用 -> 127.0.0.53 -> systemd-resolved
```

那这个问题以后还会回来。因为根因不是缓存，而是“该组件本身会拒收这类报文”。

所以这次修复的核心不是“清掉缓存”，而是“让应用不要再把 `systemd-resolved` 当成主解析入口”。

## 5. Docker 为什么会让问题更难理解

### 5.1 这不是普通的网段相似，而是实际重叠

现场状态里出现过下面这组地址：

```text
singbox_tun:        172.18.0.1/30
sing-box DNS peer:  172.18.0.2

docker_default:     172.18.0.0/16
docker bridge gw:   172.18.0.1
weaviate container: 172.18.0.2
```

也就是说，冲突不是“都在 172.18 段里”这么简单，而是：

- TUN 本地地址和 Docker bridge 网关都叫 `172.18.0.1`
- TUN 对端 DNS `172.18.0.2` 和 Docker 容器 `172.18.0.2` 也撞了

这是同一个地址对在两个逻辑网络里重复出现。

### 5.2 它为什么危险

Linux 路由查找遵循最长前缀匹配，所以单看路由项时，`172.18.0.0/30` 往往会压过 `172.18.0.0/16`。但这不代表冲突不存在，因为：

- 两个接口都声称自己拥有 `172.18.0.1`
- 不同调试工具、不同路径、不同内核行为会让现象很难一眼判断
- 某些回包或本地访问路径会呈现出非常迷惑的行为

这种情况下，即使某一次请求“恰好走对了”，它也仍然是一个不稳定的配置。

### 5.3 为什么这次直接删除 Dify 就能收口

这台机器上 `172.18.0.0/16` 的来源不是 Docker 全局守护进程不可控地乱分，而是 Dify 那组 Compose 网络自动生成的 `docker_default`。

删除 Dify 部署后：

- `docker_default` 被删除
- `docker_ssrf_proxy_network` 也一并删除
- Docker 只剩 `bridge` / `host` / `none`

因此 `172.18.0.0/16` 这一组冲突当场消失。

如果未来重新引入 Docker 项目，仍然需要避免重新创建 `172.18.0.0/16`。

## 6. 为什么 `dnsmasq -> 172.18.0.2` 可以修好，而且不会 DNS 泄露

这是这次修复里最关键、也最容易被误解的一点。

### 6.1 `127.0.0.1` 只是本地入口，不代表直连公网

修复后主机应用看到的是：

```text
/etc/resolv.conf -> nameserver 127.0.0.1
```

很多人看到 `127.0.0.1` 会下意识觉得“那后面是不是直接去问本地真实上游 DNS 了”。这个担心只有在下面这种结构里才成立：

```text
应用 -> 127.0.0.1 -> 本地 resolver -> 8.8.8.8 / 1.1.1.1 直连出网
```

这次实际采用的不是这个方案。

### 6.2 实际采用的是“本地前端 + TUN DNS 后端”

真正链路是：

```text
应用
  -> /etc/resolv.conf
  -> 127.0.0.1:53 (dnsmasq)
  -> 172.18.0.2:53 (sing-box TUN DNS)
  -> sing-box 上游 DNS 逻辑
  -> 远端 DoH / DNS
```

所以：

- 应用不再经过 `systemd-resolved`
- 但 DNS 最终仍然经过 sing-box
- 因此不会退化成“物理网卡直连 DNS”

### 6.3 为什么 `dnsmasq` 能修，而 `systemd-resolved` 不能

现场做过一个最小验证：

- 临时起一个 `dnsmasq`，监听 `127.0.0.1:1053`
- 上游只指向 `172.18.0.2`
- 再执行：

```bash
dig @127.0.0.1 -p 1053 pypi.org A
dig +noedns @127.0.0.1 -p 1053 pypi.org A
```

结果都正常，而且 `+noedns` 时也不再看到那种异常 `OPT` 暴露到客户端。

这说明 `dnsmasq` 在这里起到了两个作用：

1. 绕开了 `systemd-resolved` 的严格拒收逻辑
2. 给客户端返回了一个更稳定、更普通的应答形式

可以把它理解为：

- 它不是从根上修复上游世界的异常报文
- 它是在本机解析入口前面放了一个更合适的“缓冲层”

### 6.4 为什么这不构成 DNS 泄露

这次 `dnsmasq` 配置的关键只有一句：

```conf
server=172.18.0.2
```

只要这一点不变，就意味着：

- 主机应用看不到任何物理网卡 DNS 上游
- `dnsmasq` 不会自行决定去问 `192.168.110.1`
- 也不会直接去问公网 `8.8.8.8`
- 它只会把请求交回 sing-box TUN DNS

所以主机 DNS 的可见出口仍然受 sing-box 控制。

## 7. 这次调试为什么是“证据链式”而不是“猜”

下面这些命令之所以重要，不是因为它们常见，而是因为每一条都排除了一个误判。

## 7.1 `resolvectl query pypi.org`

作用：

- 直接测系统 resolver 看到的结果

结论：

- 如果这里失败，但 `dig @172.18.0.2` 成功，问题就在 `systemd-resolved` 和上游报文兼容性之间

## 7.2 `dig @172.18.0.2 pypi.org A`

作用：

- 绕开 `systemd-resolved`，直接问 sing-box TUN DNS

结论：

- 证明 `172.18.0.2` 不是彻底坏掉

## 7.3 `dig +noedns @172.18.0.2 pypi.org A`

作用：

- 验证是否只有 no-EDNS 路径会出现异常

结论：

- 如果回包里能看到异常 `OPT`，就说明这不是单纯超时或链路断，而是协议兼容问题

## 7.4 `dig +noedns @192.168.110.1 pypi.org A`

作用：

- 判断异常是否只存在于 sing-box

结论：

- 如果路由器 DNS 也能复现同类现象，那么 sing-box 不是唯一根因，至少说明更上游也参与了这个问题

## 7.5 `resolvectl status`

作用：

- 看系统当前实际把哪条链路当默认 DNS

结论：

- `singbox_tun` 上存在 `DNS Domain: ~.` 和 `DNS Servers: 172.18.0.2`，就说明这条链路接管的是整机解析，不是少数应用

## 7.6 `ip route` 与 `ip addr`

作用：

- 看 TUN 和 Docker 是否在地址规划层互相重叠

结论：

- 同时看到 `172.18.0.0/30 dev singbox_tun` 和 `172.18.0.0/16 dev br-*`，就已经是明确风险
- 再结合接口地址，看到 `172.18.0.1` 和 `172.18.0.2` 被两边重复使用，问题就不再是“怀疑”

## 7.7 `docker network inspect`

作用：

- 找出是谁创建了冲突网段

结论：

- 这次是 Dify 的 `docker_default` 真正在用 `172.18.0.0/16`

## 7.8 临时 `dnsmasq` 验证

作用：

- 在不改系统解析入口之前，先验证“绕开 `systemd-resolved` 是否就能恢复 `pypi.org`”

结论：

- 一旦 `dnsmasq -> 172.18.0.2` 正常，就说明修复方向是对的

## 8. 第一次修复为什么只修好了一半

截至 2026-03-13，第一次修复实际完成的是：

- `/etc/resolv.conf` 被改成 `nameserver 127.0.0.1`
- 本机启用了 `v2rayn-local-dns.service`
- `dnsmasq` 只转发到 `172.18.0.2`
- Dify 的 `docker_default` 已删除，因此当时的 `172.18.0.0/16` 冲突消失
- `poetry lock` / `poetry sync` 已恢复正常

这一步证明了“`127.0.0.1 -> dnsmasq -> 172.18.0.2` 能修好 PyPI”，但它还缺两块：

1. 没有处理 TUN 关闭后的 DNS 回退
2. 没有处理 sing-box 关闭后可能残留的策略路由

于是后来的主机状态变成：

- 代理开启时，PyPI 正常
- 代理关闭时，`dnsmasq` 仍然只认识 `172.18.0.2`
- 如果 `table 2022` 和 `9000~9010` 规则还在，公网包仍然会被扔进已经失效的 `singbox_tun`

所以用户体感就是：“代理开着能用，代理一关整机像没网。”

## 9. 第二次修补为什么要加自动回退

这次补的是控制面，不是再换一遍 DNS。

新增的目标是：

- `singbox_tun` 存在时：
  - 维持 `127.0.0.1 -> dnsmasq -> 172.18.0.2`
- `singbox_tun` 消失时：
  - 停止 `dnsmasq`
  - 恢复 `/etc/resolv.conf` 备份
  - 清掉残留的 `table 2022` 和 `ip rule 9000/9001/9002/9003/9010`

也就是说，第一次修的是“代理模式可用”，第二次修的是“代理模式和直连模式都能切换回来”。

落地方式是：

- 继续保留 `v2rayn-local-dns.service`
- 新增 `v2rayn-local-dns-sync.service`
- 新增 `v2rayn-local-dns-sync.timer`
- 新增 `/usr/local/libexec/v2rayn-local-dns-sync.sh`

其中同步脚本会定期检查 `singbox_tun` 是否存在，并据此切换主机 DNS 与路由收尾动作。

## 10. 如果以后问题再复发，优先怎么判断

先不要立即怀疑“代理挂了”。

先分层判断：

### 第一层：是应用问题，还是系统 DNS 问题

```bash
getent ahosts pypi.org
resolvectl query pypi.org
```

如果 `getent` 失败，说明应用侧也受影响；如果 `resolvectl` 失败但 `getent` 正常，说明你已经成功把应用绕开了 `systemd-resolved`。

### 第二层：是 `systemd-resolved` 问题，还是 sing-box 问题

```bash
dig @172.18.0.2 pypi.org A
dig +noedns @172.18.0.2 pypi.org A
```

如果 direct `dig` 成功，而 `resolvectl` 失败，重点看 resolver 兼容性，不要先怀疑代理节点。

### 第三层：是不是又出现 Docker 重叠

```bash
ip route
docker network ls
docker network inspect <network>
```

只要再看到 `172.18.0.0/16` 出现，就要立刻怀疑新的 Docker 项目又占了 TUN 地址段。

## 11. 未来如果重新引入 Docker，应该怎么做

当前是通过“删除 Dify 部署”直接解除冲突。以后如果重新跑 Docker 项目，建议至少满足下面之一：

1. 在 `docker-compose.yaml` 里显式为 `default` network 写 `ipam`
2. 在 `/etc/docker/daemon.json` 里设置 `default-address-pools`
3. 如果未来 v2rayN 暴露 `auto_redirect` 且整套链路验证稳定，再评估是否采用 sing-box 官方推荐的 Linux TUN 重定向方式

但不管选哪条路，一个原则不能变：

- 不要让 Docker 再碰 `172.18.0.0/16`

## 12. 这次问题真正值得记住的心智模型

这次最重要的经验不是某一条命令，而是下面这个判断顺序：

1. 先确认失败发生在“谁”身上：应用、`systemd-resolved`、还是 sing-box
2. 再确认失败属于哪一类：报文兼容性、地址规划冲突、还是代理出口不可达
3. 最后才决定修的是 resolver、DNS 上游、还是 Docker 网段

如果把这三类问题混在一起，症状会看起来像“网络玄学”。把它们拆开之后，问题实际上很清晰：

- `pypi.org` 坏，是 resolver 兼容性问题
- `172.18.0.0/16` 坏，是地址规划问题
- 修复时又不能牺牲 DNS 不泄露目标，所以要让本地前端继续把查询交回 `172.18.0.2`

这就是这次修复最终落成“`127.0.0.1 -> dnsmasq -> 172.18.0.2 -> sing-box`”而不是“换成直连公共 DNS”的原因。
