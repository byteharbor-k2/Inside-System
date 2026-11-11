# Inside-System

主机侧与系统侧知识库，记录 Ubuntu 环境配置、网络调试、脚本工具以及硬件相关笔记。

## 模块总览

- `Linux/`：启动与双系统原理
- `Linux/ubuntu/`：Ubuntu 主机环境、TUN / DNS / Docker 网络问题排查与修复
- `Linux/ubuntu/typing/`：输入法（Fcitx5 + Rime 雾凇拼音）
- `Linux/ubuntu/ai-studio/`：本地 AI 算力主机的硬件、风扇控制与终端配置
- `Linux/ubuntu/scripts/`：配套 Bash 脚本
- `hardware/`：硬件与计算体系结构相关笔记
- `OS_Spring_2026/`：2026 春操作系统课程代码与学习笔记同步

## 索引

### Linux

- [UEFI、EFI、Ventoy、ISO 与双系统原理笔记](Linux/uefi-efi-ventoy-dual-boot-primer.md)
  U 盘启动、UEFI 引导流程、Ventoy 原理与双系统安装。

#### Ubuntu 输入法

- [Ubuntu 26.04 (GNOME Wayland) 安装 Fcitx5 + Rime 雾凇拼音](Linux/ubuntu/typing/ubuntu-26.04-fcitx5-rime-wayland-setup.md)
  **当前方案。** 纯 Wayland 下的安装步骤、kimpanel 候选框修复原理、常用按键与自定义。
- [Ubuntu 输入法技术文档](Linux/ubuntu/typing/ubuntu-input-method-architecture.md)
  输入法框架 / 引擎 / 方案的分层架构、Rime 概念与 patch 机制（基于 24.04）。
- [Ubuntu 22.04 安装 Fcitx5 + Rime 雾凇拼音 & 卸载 KDE Plasma 记录](Linux/ubuntu/typing/fcitx5-rime-kde-cleanup-summary.md)
  22.04 时代的安装、Lua 兼容问题与 KDE 清理记录。

#### AI Studio 主机

- [硬件清单与风扇控制](Linux/ubuntu/ai-studio/hardware-and-fan-control.md)
  Xeon E5-2696 v3 + 128 GB + 双 RX 7900 XTX 的硬件清单、主板传感器驱动、显卡风扇控制（`ppfeaturemask`）与 CoolerControl。
- [Ghostty + Claude Code 终端配置](Linux/ubuntu/ai-studio/ghostty-claude-code-terminal.md)
  `Shift+Enter` 换行、好记版 Ghostty 快捷键、剪贴板截图粘贴（`wl-clipboard`）。

#### Ubuntu 网络

- [Ubuntu TUN / DNS / Docker 修复计划](Linux/ubuntu/tun-dns-docker-fix-plan.md)
  V2rayN TUN、sing-box DNS、Docker 地址冲突与 PyPI 解析异常的修复方案。
- [Ubuntu TUN / DNS / Docker 问题的技术原理与调试说明](Linux/ubuntu/tun-dns-docker-debug-principles.md)
  上述问题的原理、证据链、调试命令和最终架构说明。

#### Ubuntu Scripts

- [apply_v2rayn_dns_frontend.sh](Linux/ubuntu/scripts/apply_v2rayn_dns_frontend.sh)
  将主机 DNS 入口切到 `127.0.0.1 -> dnsmasq -> 172.18.0.2`，绕开 `systemd-resolved`，同时保持 DNS 经 TUN / 代理出口转发。
- [check_singbox_policy_routing.sh](Linux/ubuntu/scripts/check_singbox_policy_routing.sh)
  检查 TUN 网卡消失后残留的 sing-box 策略路由（table 2022），避免直连流量被黑洞。
- [configure_docker_address_pool.sh](Linux/ubuntu/scripts/configure_docker_address_pool.sh)
  备份并修改 `/etc/docker/daemon.json` 的默认地址池，避免与 TUN / 局域网网段冲突。
- [remove_dify_docker_data.sh](Linux/ubuntu/scripts/remove_dify_docker_data.sh)
  删除旧 Dify Docker volumes 数据目录。
- [load_motherboard_sensors.sh](Linux/ubuntu/scripts/load_motherboard_sensors.sh)
  自动识别并加载主板 Super I/O 传感器驱动（nct6775 / it87 等），设为开机加载。

### Hardware

- [The_growth_of_computer_performance.md](hardware/The_growth_of_computer_performance.md)
  计算机性能增长阶段、关键机制与多核时代背景笔记。
- [ISA_ABI_microarchitecture.md](hardware/ISA_ABI_microarchitecture.md)
  用大白话讲清 ISA、ABI、微架构三者的区别。

### OS Spring 2026

- [OS_Spring_2026/README.md](OS_Spring_2026/README.md)
  2026 春南京大学操作系统课程资料入口，包含 `course_2` 到 `course_5` 的代码同步记录。
- [course_5/README.md](OS_Spring_2026/course_5/README.md)
  第 5 讲“程序和进程”的代码同步说明，包含 `crazy-os/`、`proc-info/`、`pstree/`、`fork-dfs/`、`fork-demo/`、`execve-demo/`。

## 使用约定

- 文档优先按主题归类到顶层模块目录。
- 可执行脚本放到对应主题下的 `scripts/` 子目录。
- 涉及系统级改动的脚本默认需要先阅读内容，再手动用 `sudo bash <script>` 执行。
