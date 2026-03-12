# Ubuntu 输入法技术文档

**系统环境：** Ubuntu 24.04.4 LTS, GNOME (Xorg), ThinkBook 14 G5 IRH
**当前方案：** Fcitx5 5.1.7 + Rime 1.10.0 + 雾凇拼音 (Rime Ice)
**最后更新：** 2026-03-12

---

## 一、Linux 输入法整体架构

Linux 下输入中文需要三个层次协作：

```
┌──────────────────────────────────────────────────────────┐
│                    应用程序 (App)                          │
│            Chrome, VS Code, Terminal, ...                 │
├──────────────────────────────────────────────────────────┤
│                  输入法框架 (Framework)                     │  ← Fcitx5 / iBus
│          接收键盘事件、显示候选窗、分发给引擎                   │
├──────────────────────────────────────────────────────────┤
│                  输入法引擎 (Engine)                        │  ← Rime / libpinyin / Anthy
│           拼音拆分、词库匹配、候选排序                        │
├──────────────────────────────────────────────────────────┤
│                 配置方案 (Schema/Config)                    │  ← 雾凇拼音 / 明月拼音
│            词库、规则文件、Lua 脚本                          │
└──────────────────────────────────────────────────────────┘
```

### 为什么需要分层？

| 问题 | 解答 |
|------|------|
| 为什么不让应用直接调用输入法引擎？ | 每个 GUI 工具包 (GTK, Qt, Electron) 有不同的键盘事件模型，需要框架层统一适配 |
| 为什么引擎和方案分离？ | Rime 是通用引擎，同一引擎可以跑全拼、双拼、五笔、仓颉等完全不同的方案 |
| 为什么框架和引擎分离？ | 同一个 Fcitx5 可以同时挂载 Rime、libpinyin、Anthy(日文) 等多个引擎 |

---

## 二、输入法框架：Fcitx5 vs iBus

Ubuntu 上主流的两个输入法框架：

| 对比项 | Fcitx5 | iBus |
|--------|--------|------|
| 维护者 | CSSlayer (个人) | Red Hat / GNOME |
| Ubuntu 默认 | 否 | 是 |
| Rime 支持 | 优秀 (`fcitx5-rime`) | 可用 (`ibus-rime`) |
| Wayland 支持 | 部分 (GNOME Wayland 仍走 iBus 协议) | 原生 |
| 候选窗自定义 | 丰富 (主题、字体、位置) | 基础 |
| 响应速度 | 快 | 一般 |
| GNOME 集成 | 需手动配置环境变量 | 开箱即用 |

### 本机选择 Fcitx5 的原因

1. Rime 在 Fcitx5 下体验更好（候选窗渲染、快捷键支持）
2. 社区活跃，雾凇拼音官方推荐 Fcitx5
3. 22.04 时代 iBus 版本的 Rime 存在兼容性问题

### 本机 Fcitx5 组件清单

```
fcitx5                      核心守护进程
fcitx5-rime                 Rime 引擎插件
fcitx5-pinyin               内置拼音引擎（未使用，作为 fallback）
fcitx5-frontend-gtk3        GTK3 应用 IM Module
fcitx5-frontend-gtk4        GTK4 应用 IM Module
fcitx5-frontend-qt5         Qt5 应用 IM Module
fcitx5-frontend-qt6         Qt6 应用 IM Module
fcitx5-config-qt            图形配置工具
fcitx5-chinese-addons       中文附加组件（标点、云拼音等）
fcitx5-module-lua           Lua 脚本支持
```

### Fcitx5 如何与应用通信

```
键盘按键
    │
    ▼
┌─────────┐     XIM / D-Bus / IM Module
│  Fcitx5  │ ◄───────────────────────────── 应用程序
│  daemon  │
└────┬─────┘
     │ 将按键发送给当前激活的引擎
     ▼
┌─────────┐
│  Rime    │ → 返回候选词列表
└─────────┘
     │
     ▼
Fcitx5 渲染候选窗，将选中的文字通过 commit string 发回应用
```

通信协议取决于应用的 GUI 工具包：

| 工具包 | 通信方式 | 对应包 |
|--------|----------|--------|
| GTK3/4 | IM Module (动态库注入) | `fcitx5-frontend-gtk3/gtk4` |
| Qt5/6 | IM Module (动态库注入) | `fcitx5-frontend-qt5/qt6` |
| Electron/Chromium | 通过 GTK IM Module 或 iBus 协议 | 依赖 `GTK_IM_MODULE` 环境变量 |
| Xlib 原生 | XIM 协议 | Fcitx5 内置 |
| Wayland 原生 | zwp_input_method_v2 | `fcitx5` 内置 |

---

## 三、输入法引擎：Rime（中州韵）

### 3.1 Rime 是什么

Rime（中州韵输入法引擎）是一个**跨平台、可编程的通用输入法引擎**：

| 平台 | 前端名称 |
|------|----------|
| Linux (Fcitx5) | `fcitx5-rime` |
| Linux (iBus) | `ibus-rime` |
| macOS | 鼠须管 (Squirrel) |
| Windows | 小狼毫 (Weasel) |
| Android | 同文输入法 (Trime) |
| iOS | 仓输入法 (Hamster) |

所有平台共享同一套配置方案格式（`.yaml`），词库和方案可以跨平台通用。

### 3.2 核心概念

| 概念 | 说明 |
|------|------|
| **Schema（方案）** | 一个 `.schema.yaml` 文件定义一种输入法（如全拼、双拼、五笔） |
| **Dict（词典）** | `.dict.yaml` 文件，存储词条和权重 |
| **Deploy（部署）** | Rime 将 YAML 配置编译为二进制格式的过程，修改配置后需重新部署 |
| **Custom（定制）** | `*.custom.yaml` 文件用于覆盖默认配置，不直接修改原始文件 |
| **Lua 脚本** | 扩展 Rime 功能（日期输入、计算器等），需 librime >= 1.8.5 |

### 3.3 Rime 引擎处理流程

```
用户按键 "nihao"
    │
    ▼
Processor（处理器）
    │  判断按键类型（编码、标点、功能键）
    ▼
Segmentor（分段器）
    │  将 "nihao" 拆分为 "ni" + "hao"
    ▼
Translator（翻译器）
    │  查词库，得到 "你好" (权重 99), "拟好" (权重 30), ...
    ▼
Filter（过滤器）
    │  Emoji 追加、简繁转换、去重
    ▼
返回候选列表: ["你好", "你好 😄", "拟好", ...]
```

### 3.4 本机 librime 版本

```
librime:   1.10.0  (Ubuntu 24.04 自带，支持全部功能)
```

> **历史问题：** Ubuntu 22.04 自带 librime 1.7.3，不支持雾凇拼音的 Lua 脚本，
> 当时通过 `rime_ice.custom.yaml` 禁用了所有 Lua processor/translator。
> 升级到 24.04 后 librime 已是 1.10.0，该 workaround 可以移除（见第六节）。

---

## 四、配置方案：雾凇拼音（Rime Ice）

### 4.1 为什么用雾凇拼音

Rime 自带的"明月拼音"方案词库小、无中英混输、无 Emoji，体验远不如商业输入法。
[雾凇拼音](https://github.com/iDvel/rime-ice) 是社区维护的一套开箱即用配置，提供：

- 大规模中文词库（持续更新）
- 中英混输（输入 "hello" 直接出英文候选）
- Emoji 支持
- 模糊音支持
- 多种双拼方案
- Lua 扩展（日期、时间、农历、计算器、以词定字等）

### 4.2 目录结构

配置根目录：`~/.local/share/fcitx5/rime/`

```
~/.local/share/fcitx5/rime/
├── default.yaml                 # 全局配置（方案列表、候选词数、快捷键）
├── rime_ice.schema.yaml         # 雾凇拼音全拼方案定义
├── rime_ice.custom.yaml         # 用户定制覆盖（patch 机制）
├── double_pinyin_flypy.schema.yaml   # 小鹤双拼方案
├── double_pinyin_mspy.schema.yaml    # 微软双拼方案
├── ... (其他双拼方案)
├── melt_eng.schema.yaml         # 英文混输方案
├── melt_eng.dict.yaml           # 英文词典
├── radical_pinyin.schema.yaml   # 部件拆字方案
├── t9.schema.yaml               # 九宫格方案
├── cn_dicts/                    # 中文词库目录
│   ├── base.dict.yaml           #   基础字词
│   ├── ext.dict.yaml            #   扩展词汇
│   ├── tencent.dict.yaml        #   腾讯词向量词库
│   └── ...
├── en_dicts/                    # 英文词库目录
├── opencc/                      # 简繁转换、Emoji 映射
│   ├── emoji.json
│   └── ...
├── lua/                         # Lua 扩展脚本
│   ├── date_translator.lua      #   输入 "rq" → "2026-03-12"
│   ├── time_translator.lua      #   输入 "sj" → "13:30"
│   ├── select_character.lua     #   以词定字
│   └── ...
├── custom_phrase.txt            # 自定义短语（固定候选，不参与排序）
├── build/                       # deploy 后的编译产物（自动生成）
└── installation.yaml            # Rime 安装信息
```

### 4.3 配置定制机制

Rime 使用 **patch 机制** 进行配置定制，**永远不应直接修改原始方案文件**：

| 要修改的文件 | 创建定制文件 |
|-------------|-------------|
| `default.yaml` | `default.custom.yaml` |
| `rime_ice.schema.yaml` | `rime_ice.custom.yaml` |
| `double_pinyin_flypy.schema.yaml` | `double_pinyin_flypy.custom.yaml` |

定制文件格式：

```yaml
# rime_ice.custom.yaml 示例
patch:
  # 修改候选词数量
  "menu/page_size": 7

  # 追加模糊音规则
  "speller/algebra/+":
    - derive/^z/zh/
    - derive/^c/ch/
    - derive/^s/sh/
```

修改后需重新部署：`Ctrl+Alt+Grave`（在输入法激活状态下）或重启 Fcitx5。

### 4.4 当前启用的方案

```yaml
# default.yaml → schema_list
- rime_ice          # 雾凇拼音（全拼）← 主力方案
- t9                # 九宫格
- double_pinyin     # 自然码双拼
- double_pinyin_abc
- double_pinyin_mspy
- double_pinyin_sogou
- double_pinyin_flypy
- double_pinyin_ziguang
- double_pinyin_jiajia
```

---

## 五、环境变量与自启动

### 5.1 环境变量

位置：`~/.profile`

```bash
export GTK_IM_MODULE=fcitx      # GTK 应用使用 fcitx IM Module
export QT_IM_MODULE=fcitx       # Qt 应用使用 fcitx IM Module
export XMODIFIERS=@im=fcitx     # X11 XIM 协议指向 fcitx
export SDL_IM_MODULE=fcitx      # SDL 游戏/应用使用 fcitx
export GLFW_IM_MODULE=ibus      # GLFW 应用（部分 Electron）走 iBus 兼容层
```

> `GLFW_IM_MODULE=ibus` 看起来矛盾，但这是因为 GLFW 只支持 iBus 协议，
> 而 Fcitx5 内置了 iBus 兼容前端，所以实际仍由 Fcitx5 处理。

### 5.2 自启动

```
~/.config/autostart/org.fcitx.Fcitx5.desktop
```

该 `.desktop` 文件确保登录时自动启动 Fcitx5 守护进程。

### 5.3 输入法框架选择

通过 `im-config` 管理系统级输入法框架：

```bash
im-config -l    # 列出可用框架：ibus fcitx5 xim
im-config -n fcitx5   # 设置默认框架为 fcitx5
```

---

## 六、已知问题与维护

### 6.1 Lua 脚本 workaround（可移除）

升级到 24.04 后，librime 已从 1.7.3 升级到 1.10.0，完全支持雾凇拼音的 Lua 脚本。

当前 `~/.local/share/fcitx5/rime/rime_ice.custom.yaml` 中仍保留着 22.04 时代
禁用 Lua 的 workaround。**删除该文件可恢复以下功能：**

- 以词定字（`[` / `]` 键）
- 日期/时间输入（`rq` → 日期, `sj` → 时间）
- 农历
- 计算器（`=1+1`）
- 数字大写转换
- 错音错字提示
- 英文自动大写
- 长词优先

恢复命令：

```bash
rm ~/.local/share/fcitx5/rime/rime_ice.custom.yaml
# 重新部署 Rime（方法二选一）：
#   1. 在输入法激活状态下按 Ctrl+Alt+`
#   2. 或重启 fcitx5：
fcitx5 -r -d
```

### 6.2 Wayland 兼容性

当前使用 X11 会话（"Ubuntu on Xorg"）。如切换到 Wayland 会话：

- GNOME Wayland 下输入法通过 iBus 协议桥接，Fcitx5 通过内置的 `ibusfrontend` 模块兼容
- Ubuntu 24.04 的 Fcitx5 5.1.7 对 Wayland 支持已大幅改善，可以尝试
- 如果 Wayland 下输入法失效，回退到 Xorg 会话即可

### 6.3 雾凇拼音更新

雾凇拼音通过 Git 仓库发布更新（词库、方案、Lua 脚本），更新方法：

```bash
cd /tmp
git clone --depth 1 https://github.com/iDvel/rime-ice.git
# 备份个人定制
cp ~/.local/share/fcitx5/rime/custom_phrase.txt /tmp/custom_phrase_backup.txt
cp ~/.local/share/fcitx5/rime/*.custom.yaml /tmp/ 2>/dev/null
# 覆盖更新
cp -r /tmp/rime-ice/* ~/.local/share/fcitx5/rime/
# 恢复个人定制
cp /tmp/custom_phrase_backup.txt ~/.local/share/fcitx5/rime/custom_phrase.txt
cp /tmp/*.custom.yaml ~/.local/share/fcitx5/rime/ 2>/dev/null
# 清理并重新部署
rm -rf /tmp/rime-ice
fcitx5 -r -d
```

### 6.4 常用快捷键

| 快捷键 | 功能 |
|--------|------|
| `Ctrl+Space` | 切换中/英文输入法 |
| `Left Shift` | 中/英模式切换（Rime 内部） |
| `Ctrl+Shift+F` | 简繁切换 |
| `Ctrl+Alt+`` ` | Rime 重新部署 |
| `Ctrl+.` | 中/英标点切换 |
| `F4` 或 `` Ctrl+` `` | Rime 方案切换菜单 |

### 6.5 常用路径

| 路径 | 用途 |
|------|------|
| `~/.local/share/fcitx5/rime/` | Rime 用户配置和词库（雾凇拼音） |
| `~/.local/share/fcitx5/rime/build/` | Rime 编译产物（deploy 自动生成） |
| `~/.config/fcitx5/profile` | Fcitx5 输入法列表配置 |
| `~/.config/fcitx5/config` | Fcitx5 全局配置（快捷键等） |
| `/usr/share/rime-data/` | 系统级 Rime 数据（librime-data 包） |
| `/tmp/rime.fcitx-rime.*` | Rime 运行时日志（排错用） |
| `~/.config/autostart/org.fcitx.Fcitx5.desktop` | Fcitx5 自启动入口 |
| `~/.profile` | 输入法环境变量 |

---

## 七、故障排查

### 打不出中文

```bash
# 1. 检查 Fcitx5 是否在运行
pgrep fcitx5

# 2. 检查环境变量
env | grep -E "IM_MODULE|XMOD"

# 3. 检查 Rime 日志
cat /tmp/rime.fcitx-rime.ERROR 2>/dev/null
cat /tmp/rime.fcitx-rime.WARNING 2>/dev/null

# 4. 检查 Rime 是否部署成功
ls ~/.local/share/fcitx5/rime/build/*.bin
# 应该有 rime_ice.*.bin 等文件

# 5. 强制重新部署
rm -rf ~/.local/share/fcitx5/rime/build/
fcitx5 -r -d
```

### 特定应用无法输入中文

```bash
# 检查应用的 IM Module 加载情况
GTK_IM_MODULE=fcitx QT_IM_MODULE=fcitx XMODIFIERS=@im=fcitx <app-command>
```

常见问题应用：
- **Electron 应用** (VS Code, Slack)：确保 `GTK_IM_MODULE=fcitx` 已设置
- **Flatpak 应用**：需要额外安装 `org.fcitx.Fcitx5.Addon` Flatpak 扩展
- **Snap 应用**：部分 Snap 包无法访问主机 IM Module，无解

### 升级系统后 Rime 消失

升级 Ubuntu 大版本时 `fcitx5-rime` 可能被移除（视为废弃依赖），重新安装即可：

```bash
sudo apt install fcitx5-rime
fcitx5 -r -d
```

用户配置（`~/.local/share/fcitx5/rime/`）不会被删除，装回引擎即可恢复。
