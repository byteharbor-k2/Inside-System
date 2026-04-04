# Inside-System

主机侧与系统侧知识库，记录 Ubuntu 环境配置、网络调试、脚本工具以及硬件相关笔记。

## 模块总览

- `Linux/ubuntu/`：Ubuntu 主机环境、输入法、TUN / DNS / Docker 网络问题排查与修复
- `hardware/`：硬件与计算体系结构相关笔记
- `OS_Spring_2026/`：2026 春操作系统课程代码与学习笔记同步

## 索引

### Linux

#### Ubuntu

- [Ubuntu 输入法技术文档](Linux/ubuntu/ubuntu-input-method-architecture.md)
  Ubuntu 下输入法框架、Rime 引擎、雾凇拼音配置与兼容性说明。
- [Ubuntu 22.04 安装 Fcitx5 + Rime 雾凇拼音 & 卸载 KDE Plasma 记录](Linux/ubuntu/fcitx5-rime-kde-cleanup-summary.md)
  输入法安装、KDE 清理与兼容性问题处理记录。
- [Ubuntu TUN / DNS / Docker 修复计划](Linux/ubuntu/tun-dns-docker-fix-plan.md)
  V2rayN TUN、sing-box DNS、Docker 地址冲突与 PyPI 解析异常的修复方案。
- [Ubuntu TUN / DNS / Docker 问题的技术原理与调试说明](Linux/ubuntu/tun-dns-docker-debug-principles.md)
  上述问题的原理、证据链、调试命令和最终架构说明。

#### Ubuntu Scripts

- [apply_v2rayn_dns_frontend.sh](Linux/ubuntu/scripts/apply_v2rayn_dns_frontend.sh)
  将主机 DNS 入口切到 `127.0.0.1 -> dnsmasq -> 172.18.0.2`，绕开 `systemd-resolved`，同时保持 DNS 经 TUN / 代理出口转发。
- [remove_dify_docker_data.sh](Linux/ubuntu/scripts/remove_dify_docker_data.sh)
  删除旧 Dify Docker volumes 数据目录。

### Hardware

- [The_growth_of_computer_performance.md](hardware/The_growth_of_computer_performance.md)
  计算机性能增长阶段、关键机制与多核时代背景笔记。

### OS Spring 2026

- [OS_Spring_2026/README.md](OS_Spring_2026/README.md)
  2026 春南京大学操作系统课程资料入口，包含 `course_2` 到 `course_5` 的代码同步记录。
- [course_5/README.md](OS_Spring_2026/course_5/README.md)
  第 5 讲“程序和进程”的代码同步说明，包含 `crazy-os/`、`proc-info/`、`pstree/`、`fork-dfs/`、`fork-demo/`、`execve-demo/`。

## 使用约定

- 文档优先按主题归类到顶层模块目录。
- 可执行脚本放到对应主题下的 `scripts/` 子目录。
- 涉及系统级改动的脚本默认需要先阅读内容，再手动用 `sudo bash <script>` 执行。
