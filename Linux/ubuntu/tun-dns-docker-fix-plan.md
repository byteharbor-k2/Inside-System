# Ubuntu TUN / DNS / Docker 修复计划

更新时间：2026-03-12

## 1. 已确认的问题

- `pypi.org` 当前不是单纯缓存故障。主机通过 `systemd-resolved` 查询会报 `Received invalid reply`，但直接查询 TUN DNS 可以返回正常结果。
- `dig +noedns` 对 `172.18.0.2` 和 `192.168.110.1` 查询 `pypi.org A` 时，回包都带异常 `OPT`。这说明当前故障点在 `systemd-resolved` 和上游 no-EDNS 回包的兼容性。
- `singbox_tun` 当前使用 `172.18.0.1/30`，Docker bridge 同时占用 `172.18.0.0/16`。这是明确的地址重叠，不是“可能冲突”。
- 当前整机默认路由和默认 DNS 都被 TUN 接管，因此 DNS 异常会被放大为整机故障，而不是单个应用故障。

## 2. 修复目标

- `pypi.org`、`files.pythonhosted.org`、`pythonhosted.org` 在 TUN 开启和关闭两种状态下都能稳定解析。
- Docker 不再创建任何 `172.18.0.0/16` 网桥。
- TUN、Docker、主机 DNS 三者职责分离，不再相互覆盖地址和控制面。
- 主机 DNS 不再依赖 `systemd-resolved -> singbox_tun -> 上游` 这条不稳定链路。

## 3. 最终架构

- Docker 统一迁移到独立地址池，例如 `10.200.0.0/16` 和 `10.201.0.0/16`。
- TUN 保留独立 /30 网段。
  如果 V2rayN 仍硬编码 `172.18.0.1/30`，就把整段 `172.18.0.0/30` 视为 TUN 保留地址，Docker 永远不要再占用 `172.18.0.0/16`。
- 主机应用统一查询本地 resolver（推荐 `127.0.0.1`），由本地 resolver 决定上游 DNS。
- 本地 resolver 不直连公网 DNS，而是只转发到 `172.18.0.2` 这个 sing-box TUN DNS。这样既绕开 `systemd-resolved`，又保持 DNS 仍经 sing-box / 代理出口。

## 4. 执行计划

### Phase 1: 先消除 Docker / TUN 地址重叠

1. 修改 Docker 地址池。

   `/etc/docker/daemon.json` 示例：

   ```json
   {
     "default-address-pools": [
       { "base": "10.200.0.0/16", "size": 24 },
       { "base": "10.201.0.0/16", "size": 24 }
     ]
   }
   ```

2. 重建 Docker 网络。

   目标不是“新增网络可用”，而是“现有 `172.18.0.0/16` bridge 消失”。已有容器和 compose 网络需要重建，否则旧 bridge 会继续保留。

3. 验证主路由表。

   修复后应满足：

   - 允许存在 `172.18.0.0/30 dev singbox_tun`
   - 不允许再存在 `172.18.0.0/16 dev br-*`

### Phase 2: 把主机 DNS 从 `systemd-resolved` 故障链路上拿下来

推荐方案：保留 TUN 负责转发流量，但主机 DNS 统一切到本地 forwarder，例如 `dnsmasq` 监听 `127.0.0.1`，并且只把请求转发到 `172.18.0.2`。

原因：

- 当前故障是 `systemd-resolved` 在 no-EDNS 路径上拒收异常回包。
- 只做 `flush-caches` 或重启 TUN 只能临时缓解，不能消除同类故障。
- 只给 `pypi.org` 打补丁可以先止血，但主机默认解析路径仍然不稳。

本地 forwarder 的目标配置：

- 监听：`127.0.0.1`
- 默认上游：`172.18.0.2`
- 由 sing-box 决定远端 DNS 如何经代理出口访问
- 不把 `192.168.110.1` 或任何公网 DNS 作为主机应用可见的直接上游

`dnsmasq` 示例：

```conf
no-resolv
listen-address=127.0.0.1
bind-interfaces
cache-size=1000
server=172.18.0.2
edns-packet-max=1232
clear-on-reload
```

主机解析入口切换到本地 resolver：

- 让 `/etc/resolv.conf` 指向 `127.0.0.1`
- 不再把 `127.0.0.53` 作为主机应用的活跃入口

静态 `/etc/resolv.conf` 示例：

```conf
nameserver 127.0.0.1
options timeout:2 attempts:2
```

备注：

- 如果你仍把应用入口留在 `127.0.0.53`，那 `systemd-resolved` 的降级探测和异常回包拒收仍然会回来。
- 这个方案的关键不是“本地 DNS”，而是“本地 DNS 只代理到 `172.18.0.2`”，因此不会把 DNS 直连到物理网卡上游。

### Phase 3: 缩小 sing-box / TUN 的职责

- TUN 负责默认路由和流量转发。
- 主机 resolver 负责 DNS 上游选择。
- 不再把 “TUN 自动接管 DNS” 当成长期设计。

如果未来 V2rayN 支持自定义 TUN 地址，建议直接改成不落在 Docker 常见地址段内的独立网段，例如 `10.253.0.1/30`。如果做不到，就维持 `172.18.0.0/30` 仅供 TUN 使用，并把 Docker 永久迁出。

## 5. 迁移顺序

1. 先改 Docker 地址池并重建网络。
2. 再部署本地 resolver，并把 `/etc/resolv.conf` 切到 `127.0.0.1`。
3. 之后再重启 TUN / sing-box，确认它不再控制主机默认 DNS。
4. 最后再处理应用层代理设置，例如 `pip`、`curl`、浏览器是否需要继续通过代理端解析。

这个顺序的原因是：如果先改 DNS，再在地址重叠的环境里验证，会把“协议兼容问题”和“路由冲突”混在一起，验收结果不可靠。

## 6. 验收标准

### DNS

- `getent ahosts pypi.org` 返回 IPv4/IPv6 记录，不报 `Temporary failure in name resolution`
- `getent ahosts files.pythonhosted.org` 返回记录
- `curl -I https://pypi.org/simple/` 返回 HTTP 响应
- TUN 开启和关闭各测一次，结果一致

### 路由

- `ip route` 中不再出现任何 `172.18.0.0/16 dev br-*`
- `ip route` 中只保留 TUN 的 `172.18.0.0/30`，或者保留你新指定的独立 TUN 网段

### Docker

- `docker network inspect` 中所有自定义 bridge 都落在新的地址池
- 重启 Docker 后不再自动生成 `172.18.0.0/16`

## 7. 不再采用的做法

- 不再把 `sudo resolvectl flush-caches` 当成主修复方案
- 不再把“切代理后暂时恢复”当成问题已解决
- 不再接受 Docker 和 TUN 共享 `172.18.0.1`
- 不再依赖路由器 DNS 作为 PyPI 解析的最终上游

## 8. 一句话决策

长期方案只有两件事：

1. 把 Docker 永久迁出 `172.18.0.0/16`
2. 把主机 DNS 永久迁出 `systemd-resolved -> singbox_tun` 这条链路

这两件事同时完成后，`pypi.org` 故障和 TUN / Docker 混乱才算真正收口。
