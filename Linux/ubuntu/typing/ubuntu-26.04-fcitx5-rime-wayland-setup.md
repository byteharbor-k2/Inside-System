# Ubuntu 26.04 (GNOME Wayland) 安装 Fcitx5 + Rime 雾凇拼音

**操作日期：** 2026-11-11
**系统环境：** Ubuntu 26.04.1 LTS, GNOME 50 (仅 Wayland), 华南 X99-TF AI Studio 主机
**软件版本：** Fcitx5 5.1.19, fcitx5-rime 5.1.13, librime 1.16 (含 `librime-plugin-lua`), kimpanel 扩展 v92

> 架构原理见 [Ubuntu 输入法技术文档](ubuntu-input-method-architecture.md)；
> 22.04 时代的安装和踩坑记录见 [fcitx5-rime-kde-cleanup-summary.md](fcitx5-rime-kde-cleanup-summary.md)。

---

## 一、和 22.04 / 24.04 相比的变化

| 项目 | 旧做法（22.04 / 24.04） | 26.04 新做法 | 原因 |
|------|------------------------|-------------|------|
| 会话类型 | 登录时选 "Ubuntu on Xorg" 绕开 Wayland 问题 | **只能用 Wayland** | GNOME 50 已移除 X11 会话 |
| 环境变量 | 写在 `~/.profile` | 写在 `~/.config/environment.d/` | GNOME Wayland 会话由 systemd 用户实例启动，不读 `~/.profile` |
| Lua 脚本 | 22.04 librime 1.7.3 不支持，用 `rime_ice.custom.yaml` 禁用 | 安装 `librime-plugin-lua`，**不需要 workaround** | librime 1.16 + 独立 Lua 插件包 |
| 候选框位置 | Xorg 下正常 | 需要 **kimpanel** GNOME 扩展 | Wayland 下普通程序不能指定窗口坐标 |
| 雾凇拼音更新 | 拷贝覆盖 + 手动备份/恢复定制 | 保留 `.git`，直接 `git pull` | 定制只写在 `*.custom.yaml`，不冲突 |

---

## 二、安装步骤

### 2.1 安装软件包（需要 sudo）

```bash
sudo apt install -y fcitx5 fcitx5-rime librime-plugin-lua fcitx5-chinese-addons \
  fcitx5-frontend-gtk3 fcitx5-frontend-gtk4 fcitx5-frontend-qt5 fcitx5-frontend-qt6 \
  fcitx5-config-qt
```

> `librime-plugin-lua` 是单独的包，漏装会导致雾凇拼音的 Lua 功能（日期、计算器、以词定字等）失效。

### 2.2 雾凇拼音配置（保留 git 仓库，方便更新）

```bash
mkdir -p ~/.local/share/fcitx5/rime
cd ~/.local/share/fcitx5/rime
git clone --depth 1 https://github.com/iDvel/rime-ice.git .
```

以后更新：

```bash
cd ~/.local/share/fcitx5/rime && git pull
fcitx5 -r -d
```

### 2.3 环境变量（Wayland 会话）

`~/.config/environment.d/90-fcitx5.conf`：

```ini
# Fcitx5 input method (GNOME Wayland session)
XMODIFIERS=@im=fcitx
QT_IM_MODULE=fcitx
GTK_IM_MODULE=fcitx
SDL_IM_MODULE=fcitx
GLFW_IM_MODULE=ibus
```

### 2.4 Fcitx5 输入法列表

`~/.config/fcitx5/profile`（英文键盘 + Rime，`Ctrl+Space` 切换）：

```ini
[Groups/0]
Name=Default
Default Layout=us
DefaultIM=rime

[Groups/0/Items/0]
Name=keyboard-us
Layout=

[Groups/0/Items/1]
Name=rime
Layout=

[GroupOrder]
0=Default
```

### 2.5 自启动 + 切换框架

```bash
mkdir -p ~/.config/autostart
cat > ~/.config/autostart/org.fcitx.Fcitx5.desktop <<'EOF'
[Desktop Entry]
Name=Fcitx 5
Type=Application
Exec=fcitx5 -d
X-GNOME-AutoRestart=false
X-GNOME-Autostart-Notify=false
EOF

im-config -n fcitx5          # 写入 ~/.xinputrc，用户级，不需要 sudo
```

然后**注销并重新登录**。

### 2.6 安装 kimpanel 扩展（修复候选框位置）

登录后 Fcitx 会弹出 "Wayland Diagnose" 通知，建议安装
[Input Method Panel (kimpanel)](https://extensions.gnome.org/extension/261/kimpanel/)。

**方式 A：命令行（最省事，不需要浏览器插件）**

```bash
gdbus call --session --dest org.gnome.Shell.Extensions \
  --object-path /org/gnome/Shell/Extensions \
  --method org.gnome.Shell.Extensions.InstallRemoteExtension kimpanel@kde.org
# 屏幕上弹出确认框 → 点 Install
# 命令可能返回 NoReply 错误，忽略即可，用下面的命令确认结果
gnome-extensions info kimpanel@kde.org     # 应显示 Enabled: Yes, State: ACTIVE
```

**方式 B：网页安装**

需要两样东西都装上：Firefox 插件 "GNOME Shell integration" + 系统包 `gnome-browser-connector`。

```bash
sudo apt install -y gnome-browser-connector
# 完全关闭并重开 Firefox，打开扩展页面，把开关拨到 ON
```

---

## 三、原理：浏览器插件 + connector + kimpanel

### 3.1 安装通道（只在装扩展时用一次）

```
extensions.gnome.org 网页
   │ ① 网页请求安装
   ▼
Firefox 插件 (GNOME Shell integration)
   │ ② 浏览器 native messaging
   ▼
gnome-browser-connector（本地程序）
   │ ③ D-Bus
   ▼
GNOME Shell
   ④ 弹确认框 → 下载 zip → 解压到 ~/.local/share/gnome-shell/extensions/ → 启用
```

网页出于安全限制不能直接改系统，所以要插件 + connector 两层转手。缺任何一个，网页开关都没反应。
方式 A 的 `gdbus` 命令就是**跳过 ①②③，直接做第 ④ 步**。

### 3.2 候选框为什么变准了

Wayland 的安全设计：**普通程序不能自己决定窗口在屏幕上的坐标**（防止伪造弹窗）。

```
装 kimpanel 之前：
  按键 → GNOME Shell → fcitx5（算候选词）→ fcitx5 自己开窗口画候选框
                                           ✗ 无权指定坐标 → 飘到屏幕中间

装 kimpanel 之后：
  按键 → GNOME Shell → fcitx5（算候选词）
                         │ D-Bus: org.kde.kimpanel.inputmethod
                         ▼
               kimpanel 扩展（运行在 GNOME Shell 内部）
                         │ GNOME Shell 知道光标位置，有权在任意位置绘制
                         ▼
                   候选框出现在光标旁 ✓
```

分工：**fcitx5 只负责“算”，GNOME 负责“画”。** 协议名带 KDE 是因为这套协议最早由 KDE 定义。

验证两端都在线：

```bash
busctl --user list | grep -i impanel
# org.kde.impanel               gnome-shell   ← 负责画
# org.kde.kimpanel.inputmethod  fcitx5        ← 负责算
```

---

## 四、日常使用

### 4.1 常用按键

| 操作 | 按键 |
|---|---|
| 切换 fcitx5 输入法（英文键盘 ↔ Rime） | `Ctrl+Space` |
| Rime 内临时中/英（已打的拼音以英文上屏） | 左 `Shift` |
| 以词定字（取首字 / 尾字） | `[` / `]` |
| 翻页 | `-` `=` 或 `,` `.` |
| 方案菜单（简繁、全角、双拼切换） | `F4` 或 `` Ctrl+` `` |
| 重新部署 | `` Ctrl+Alt+` `` 或 `fcitx5 -r -d` |

### 4.2 雾凇拼音特殊输入（需要 Lua 插件）

| 输入 | 结果 |
|---|---|
| `rq` / `sj` / `xq` | 日期 / 时间 / 星期 |
| `dt` / `ts` | ISO 8601 时间 / Unix 时间戳 |
| `nl` | 农历 |
| `uuid` | UUID |
| `R1234.5` | 数字、金额大写 |
| `cC1+2*3` | 计算器 |
| `uU` + 部件拼音 | 部件拆字反查 |
| `U4e2d` | Unicode 码点输入 |

### 4.3 自定义（只写 `*.custom.yaml`，不改原文件）

`~/.local/share/fcitx5/rime/default.custom.yaml`：

```yaml
patch:
  menu/page_size: 7                                # 候选个数（默认 5）
  ascii_composer/switch_key/Shift_L: commit_code   # 左 Shift：拼音以英文上屏并切换
  ascii_composer/switch_key/Shift_R: noop
  schema_list:
    - schema: rime_ice
    # - schema: double_pinyin_flypy
```

`~/.local/share/fcitx5/rime/rime_ice.custom.yaml`（模糊音示例）：

```yaml
patch:
  speller/algebra/+:
    - derive/^([zcs])h/$1/
    - derive/^([zcs])([^h])/$1h$2/
    - derive/([ei])n$/$1ng/
    - derive/([ei])ng$/$1n/
```

自定义短语：`custom_phrase.txt`，每行 `文字<Tab>编码<Tab>权重`。

外观（字体、字号、主题）：`fcitx5-configtool` → 附加组件 → 经典用户界面。

---

## 五、排查清单

```bash
pgrep -a fcitx5                                   # fcitx5 是否运行
busctl --user status org.freedesktop.IBus | grep Comm=   # 应为 fcitx5（接管了 ibus 接口）
fcitx5-remote -n                                  # 当前引擎，应为 rime
ls ~/.local/share/fcitx5/rime/build/*.bin         # Rime 部署产物
cat /tmp/rime.fcitx-rime.ERROR                    # Rime 错误日志
gnome-extensions info kimpanel@kde.org            # kimpanel 状态
systemctl --user show-environment | grep -E 'IM|XMOD'   # 会话环境变量
```

| 现象 | 原因 / 处理 |
|---|---|
| 候选框飘在屏幕中间 | 没装或没启用 kimpanel（见 2.6） |
| 在 Claude Code 等 TUI 里候选框位置偏 | TUI 自绘光标，终端上报的光标位置不准；不影响输入 |
| 日期、计算器等不出 | 缺 `librime-plugin-lua` |
| 环境变量不生效 | 写到了 `~/.profile`；应放 `~/.config/environment.d/` 后重新登录 |
