# Ubuntu 22.04 安装 Fcitx5 + Rime 雾凇拼音 & 卸载 KDE Plasma 记录

**操作日期：** 2026-02-16
**系统环境：** Ubuntu 22.04.5 LTS, GNOME, ThinkBook 14 G5 IRH

---

## 一、安装 Fcitx5 + Rime 雾凇拼音输入法

### 1.1 背景

原系统使用 iBus 默认输入法，体验不佳。选择 Fcitx5 + Rime + 雾凇拼音方案，原因：
- 完全开源，本地运行，零数据上传，隐私安全
- 雾凇拼音自带中英混输 + 英文单词联想（melt_eng 模块）
- 词库丰富，智能度高

### 1.2 安装步骤

```bash
# 1. 安装 fcitx5 核心、Rime 引擎、前端模块
sudo apt install -y fcitx5 fcitx5-rime fcitx5-frontend-gtk3 fcitx5-frontend-gtk4 fcitx5-frontend-qt5

# 2. 克隆雾凇拼音配置
mkdir -p ~/.local/share/fcitx5/rime
git clone --depth 1 https://github.com/iDvel/rime-ice.git /tmp/rime-ice
cp -r /tmp/rime-ice/* ~/.local/share/fcitx5/rime/
rm -rf /tmp/rime-ice

# 3. 设置环境变量（追加到 ~/.profile）
# 添加以下内容：
#   export GTK_IM_MODULE=fcitx
#   export QT_IM_MODULE=fcitx
#   export XMODIFIERS=@im=fcitx
#   export SDL_IM_MODULE=fcitx
#   export GLFW_IM_MODULE=ibus

# 4. 设置 Fcitx5 开机自启
cp /usr/share/applications/org.fcitx.Fcitx5.desktop ~/.config/autostart/

# 5. 切换系统默认输入法框架为 fcitx5
im-config -n fcitx5

# 6. 移除 GNOME 中残留的 iBus 输入源
gsettings set org.gnome.desktop.input-sources sources "[('xkb', 'us')]"
```

### 1.3 关键问题：librime 版本过旧导致 Lua 脚本报错

**问题现象：** Fcitx5 能切换到 Rime，但输入拼音无法出中文字。

**根本原因：** Ubuntu 22.04 自带 librime 版本为 1.7.3，而雾凇拼音要求 librime >= 1.8.5。旧版 librime 不支持雾凇拼音使用的 Lua API，导致每次按键都报错：
```
LuaProcessor::ProcessKeyEvent of *select_character error(2): attempt to call a nil value
```

错误日志位置：`/tmp/rime.fcitx-rime.ERROR`

**解决方案：** 创建 `~/.local/share/fcitx5/rime/rime_ice.custom.yaml`，禁用所有 Lua 脚本：

```yaml
patch:
  "engine/processors":
    - ascii_composer
    - recognizer
    - key_binder
    - speller
    - punctuator
    - selector
    - navigator
    - express_editor
  "engine/translators":
    - punct_translator
    - script_translator
    - table_translator@custom_phrase
    - table_translator@melt_eng
    - table_translator@cn_en
    - table_translator@radical_lookup
  "engine/filters":
    - reverse_lookup_filter@radical_reverse_lookup
    - simplifier@emoji
    - simplifier@traditionalize
    - uniquifier
```

禁用后需重启 Fcitx5（`killall fcitx5 && fcitx5 -d`）使 Rime 重新 deploy。

**注意：** 禁用 Lua 后以下功能不可用：以词定字、日期/时间输入、农历、UUID、计算器、数字大写、错音提示、英文自动大写、长词优先等。核心的拼音输入、英文混输、Emoji、简繁切换功能正常。

**后续优化：** 若需恢复完整功能，需升级 librime 到 >= 1.8.5（可通过 PPA 或源码编译）。

### 1.4 关键问题：GNOME Wayland 下 Fcitx5 无法接收输入

**问题现象：** 在 Wayland 会话下，Fcitx5 后端正常运行，但应用程序无法通过 Fcitx5 输入中文。

**根本原因：** Ubuntu 22.04 的 GNOME 42 在 Wayland 下通过 iBus 协议管理输入法，会绕过 Fcitx5 的原生前端。诊断信息显示所有 InputContext 均使用 `frontend:ibus` 而非 Fcitx5 的原生 wayland 前端。

**解决方案：** 注销后在登录界面选择 **"Ubuntu on Xorg"** 而非 "Ubuntu"（Wayland）。X11 下 Fcitx5 可直接工作。

### 1.5 使用说明

- **切换中英文：** `Ctrl+Space` 或 左`Shift`
- **配置工具：** `fcitx5-configtool`
- **Rime 配置目录：** `~/.local/share/fcitx5/rime/`

---

## 二、卸载 KDE Plasma

### 2.1 背景

系统曾安装 kubuntu-desktop（KDE Plasma 桌面），后来切回 GNOME 但未完全卸载，残留的 KDE 包和 xdg-desktop-portal-kde 可能导致输入法等功能异常。

### 2.2 卸载步骤

```bash
# 第一步：卸载 KDE 核心包
sudo apt purge kubuntu-desktop plasma-desktop sddm kwin-x11 kwin-wayland kwin-common \
  dolphin konsole kate konversation ktorrent okular gwenview ark \
  elisa spectacle kinfocenter partitionmanager plasma-workspace plasma-discover kdeconnect

# 第二步：清理自动安装的依赖
sudo apt autoremove --purge
# ⚠️ 此步骤意外删除了 gdm3，导致重启后进入纯 CLI 环境
# 修复：手动启动 gdm3
#   sudo systemctl start gdm3

# 第三步：清理残留的 KDE 工具和库
sudo apt purge apport-kde kde-cli-tools kde-cli-tools-data kded5 \
  kubuntu-notification-helper debconf-kde-data debconf-kde-helper \
  libdebconf-kde1 libkworkspace5-5 polkit-kde-agent-1 qapt-batch \
  plymouth-theme-kubuntu-logo plymouth-theme-kubuntu-text libkubuntu1 \
  libreoffice-style-breeze
sudo apt autoremove --purge

# 第四步：清理 KDE SSH/Wallet 和 Wayland 集成
sudo apt purge ksshaskpass libkwalletbackend5-5
sudo apt autoremove --purge

# 第五步：清理 KDE 剪贴板和 Wayland 集成
sudo apt purge kwayland-integration copyq copyq-plugins
sudo apt autoremove --purge
```

### 2.3 注意事项

- **GDM3 被误删问题：** `sudo apt autoremove --purge` 可能将 gdm3 一并删除（因为它被标记为自动安装）。如果重启后无桌面，在 TTY 中运行：
  ```bash
  sudo apt install gdm3
  sudo systemctl start gdm3
  ```
- **保留的共享库：** 以下包被 Fcitx5 配置工具（fcitx5-config-qt）或 GNOME 工具依赖，不应删除：
  - `libkf5itemviews5`, `libkf5widgetsaddons5` → fcitx5-config-qt 依赖
  - `libblockdev*` → GNOME Disks 等磁盘管理工具依赖
- **登录界面：** SDDM 已删除，系统使用 GDM3 作为登录管理器。确认 GDM3 正常运行：
  ```bash
  systemctl status gdm3
  ```

---

## 三、最终系统状态

- **桌面环境：** GNOME（Ubuntu on Xorg）
- **登录管理器：** GDM3
- **输入法框架：** Fcitx5
- **输入法引擎：** Rime（雾凇拼音，Lua 脚本已禁用）
- **KDE 残留：** 仅剩 fcitx5-config-qt 和 GNOME 工具的共享底层库
