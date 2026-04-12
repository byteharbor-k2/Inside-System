# Ubuntu TUN / DNS / Docker 完整修复计划

更新时间：2026-04-12

## 1. 当前结论

这次问题已经分成两部分处理：

1. `pypi.org` / `files.pythonhosted.org` 解析失败的问题已经通过本地 `dnsmasq -> 172.18.0.2` 绕过 `systemd-resolved` 修掉。
2. 但第一次修复只做了“代理开启时可用”，没有做“代理关闭时自动回退”。因此后来出现了两个遗留问题：
   - 关闭 `sing-box TUN` 后，`/etc/resolv.conf` 仍然指向 `127.0.0.1`，而本地 `dnsmasq` 的唯一上游仍是 `172.18.0.2`，导致 DNS 立即失效。
   - 如果 `sing-box` 停止时没有顺手撤掉 `ip rule 9000/9001/9002/9003/9010` 和 `table 2022`，公网流量仍可能被送往已经失效的 `singbox_tun`，从而表现为“整机没网”。

所以这次的目标不是再修一次 PyPI，而是把上一次的半套方案补成真正闭环。

## 2. 修复目标

- `sing-box TUN` 开启时：
  - 主机 DNS 走 `127.0.0.1 -> dnsmasq -> 172.18.0.2 -> sing-box`
  - 保持 DNS 不泄露
  - `pypi.org`、`files.pythonhosted.org` 正常
- `sing-box TUN` 关闭时：
  - `/etc/resolv.conf` 自动恢复到执行脚本前的备份状态
  - 残留的 sing-box 策略路由自动清理
  - 主机回到手机热点 / Wi-Fi 的直连模式
- Docker 长期不再创建 `172.18.0.0/16` 自定义 bridge 网络

## 3. 最终架构

### 3.1 TUN 开启时

```text
应用
  -> /etc/resolv.conf
  -> 127.0.0.1:53
  -> dnsmasq
  -> 172.18.0.2
  -> sing-box DNS / 代理出口
```

特点：

- 不经过 `systemd-resolved` 的活跃查询路径
- 不把 DNS 直连到手机热点或运营商 DNS
- 继续满足“全局 TUN 时不泄露 DNS”

### 3.2 TUN 关闭时

```text
应用
  -> /etc/resolv.conf（恢复到原始备份）
  -> systemd-resolved / NetworkManager 当前链路 DNS
  -> 手机热点网关 / 当前网络上游
```

同时：

- `v2rayn-local-dns.service` 停止
- `table 2022` 被清空
- `ip rule 9000/9001/9002/9003/9010` 被清理

这时主机就是普通直连主机，不再依赖 `sing-box`

## 4. 落地脚本

### 4.1 DNS 自动切换

脚本：`scripts/apply_v2rayn_dns_frontend.sh`

它现在不再只是“一次性把 `/etc/resolv.conf` 改成 `127.0.0.1`”，而是会安装一整套受管控的组件：

- `v2rayn-local-dns.service`
  - 只在 TUN 开启时运行 `dnsmasq`
- `v2rayn-local-dns-sync.service`
  - 一次执行的同步服务
- `v2rayn-local-dns-sync.timer`
  - 每 5 秒检查一次 `singbox_tun` 是否存在
- `/usr/local/libexec/v2rayn-local-dns-sync.sh`
  - 负责在 TUN 模式和直连模式之间切换

同步脚本的行为：

- 如果发现 `singbox_tun` 存在：
  - 确保 `/etc/resolv.conf` 指向 `127.0.0.1`
  - 确保 `dnsmasq` 启动
  - `dnsmasq` 只转发到 `172.18.0.2`
- 如果发现 `singbox_tun` 不存在：
  - 停止 `dnsmasq`
  - 把 `/etc/resolv.conf` 恢复成初次安装前的备份
  - 清理残留的 sing-box 策略路由

这样，TUN 开与关都能自动切换，不再需要手工改 DNS。

### 4.2 Docker 永久迁出 `172.18.0.0/16`

脚本：`scripts/configure_docker_address_pool.sh`

它会修改 `/etc/docker/daemon.json`，把 Docker 自定义 bridge 网络地址池固定到：

```json
{
  "default-address-pools": [
    { "base": "10.200.0.0/16", "size": 24 },
    { "base": "10.201.0.0/16", "size": 24 }
  ]
}
```

说明：

- Docker 默认的 `bridge` 网络仍然可能保留 `172.17.0.0/16`，这是正常的
- 关键是以后新建的 compose / 自定义 bridge 不再落到 `172.18.0.0/16`
- 已存在的旧自定义网络不会自动改地址，仍然需要重建对应 compose 项目

### 4.3 手工核对策略路由残留

脚本：`scripts/check_singbox_policy_routing.sh`

用途：

- 在你手工关闭 `sing-box TUN` 后，快速判断 `table 2022` 和 `9000~9010` 这些规则是否还残留
- 如果脚本在 `tun_present=no` 时仍看到 `table 2022` 或这些 `ip rule`，就说明 sing-box 关闭后没有清干净

正常状态应该是：

- `tun_present=yes` 时，这些规则存在
- `tun_present=no` 时，这些规则不存在

## 5. 执行顺序

1. 先执行 DNS 管理脚本，装上自动回退机制
2. 再执行 Docker 地址池脚本，永久迁出 `172.18.0.0/16`
3. 手工关闭一次 `sing-box TUN`
4. 立刻执行 `check_singbox_policy_routing.sh`
5. 再核对 DNS 是否恢复成备份状态

推荐命令：

```bash
sudo bash scripts/apply_v2rayn_dns_frontend.sh
sudo bash scripts/configure_docker_address_pool.sh
bash scripts/check_singbox_policy_routing.sh
```

## 6. 验收标准

### 6.1 TUN 开启时

- `cat /etc/resolv.conf` 返回 `nameserver 127.0.0.1`
- `systemctl is-active v2rayn-local-dns` 返回 `active`
- `getent ahosts pypi.org` 正常返回
- `ip route get 1.1.1.1` 命中 `table 2022` / `singbox_tun`

### 6.2 TUN 关闭时

- `cat /etc/resolv.conf` 不再是 `127.0.0.1`，而是恢复到备份内容或原始 symlink
- `systemctl is-active v2rayn-local-dns` 返回 `inactive`
- `bash scripts/check_singbox_policy_routing.sh` 返回 `stale_singbox_policy_routing=no`
- `ip route get 1.1.1.1` 不再命中 `table 2022`
- 浏览器和 `curl` 可直接通过手机热点访问公网

### 6.3 Docker

- 新建的 compose 项目不再生成 `172.18.0.0/16` 自定义 bridge
- `docker network inspect` 里新网络应落在 `10.200.0.0/16` 或 `10.201.0.0/16`

## 7. 这次补齐后，哪些问题算真正收口

- `pypi.org` 不再因为 `systemd-resolved` 的异常回包兼容性而失败
- 关闭代理后，电脑不再因为 DNS 悬空或策略路由残留而“像断网”
- Docker 不再反复和 `singbox_tun 172.18.0.0/30` 撞地址

## 8. 一句话决策

第一次修的是“代理开启时 PyPI 可用”；这一次补的是“代理关闭时主机能自动回到直连”，并且从 Docker 守护进程层面封住 `172.18.0.0/16` 冲突的复发路径。
