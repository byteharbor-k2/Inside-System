# Ghostty + Claude Code 终端配置（Ubuntu 26.04 Wayland）

**记录日期：** 2026-11-11

---

## 一、为什么换 Ghostty

GNOME 自带终端（VTE）里，`Shift+Enter` 和 `Enter` 发出的是同一个按键序列，
Claude Code 分不出来，`Shift+Enter` 会直接发送消息。

Ghostty 支持 kitty keyboard protocol，能区分两者，Claude Code 里 **`Shift+Enter` 换行开箱即用**
（Kitty、WezTerm、iTerm2 同理）。

```bash
sudo apt install -y ghostty      # 26.04 官方源自带
```

不换终端时的换行方法：行尾输入 `\` 再回车，或 `Ctrl+J`。

---

## 二、好记版快捷键（Alt ≈ Mac 的 Cmd）

`~/.config/ghostty/config.ghostty`：

```ini
# 忘了快捷键？按 Ctrl+Shift+P 打开命令面板，输入功能名搜索

# 标签页
keybind = alt+t=new_tab
keybind = alt+w=close_surface
keybind = alt+n=new_window
# Alt+1~9 切换标签页（Ghostty 默认已有）

# 分屏（和 iTerm2 一样：D 横分，Shift+D 竖分）
keybind = alt+d=new_split:right
keybind = alt+shift+d=new_split:down
keybind = alt+left=goto_split:left
keybind = alt+right=goto_split:right
keybind = alt+up=goto_split:up
keybind = alt+down=goto_split:down
keybind = alt+z=toggle_split_zoom
keybind = alt+equal=equalize_splits

# 选中即复制；Ctrl+Shift+V 粘贴文字
# （Ctrl+C 留给终端中断程序；Ctrl+V 留给 Claude Code 贴图片）
copy-on-select = clipboard
```

修改后 `Ctrl+Shift+,` 重新加载，默认快捷键仍可用。

| 功能 | 快捷键 |
|---|---|
| 新标签页 / 关闭 / 新窗口 | `Alt+T` / `Alt+W` / `Alt+N` |
| 切换标签页 | `Alt+1～9` |
| 左右分屏 / 上下分屏 | `Alt+D` / `Alt+Shift+D` |
| 分屏间移动 | `Alt+方向键` |
| 放大还原分屏 / 均分 | `Alt+Z` / `Alt+=` |
| 复制 / 粘贴文字 | 选中即复制 / `Ctrl+Shift+V` |
| 命令面板 | `Ctrl+Shift+P` |

---

## 三、Claude Code 里粘贴截图

现象：系统截图已复制到剪贴板，在 Claude Code 里 `Ctrl+V` 提示 "No image"。

原因：Claude Code 在 Linux 上通过 `wl-paste`（Wayland）或 `xclip`（X11）读取剪贴板图片，系统默认两者都没装。

```bash
sudo apt install -y wl-clipboard
```

装好后重启 Claude Code，用 **`Ctrl+V`**（不是 `Ctrl+Shift+V`，那是终端的文字粘贴）。

---

## 四、Claude Code 里的 sudo

`!` 前缀执行的命令没有可交互的 TTY，`sudo` 会报
`sudo: A terminal is required to authenticate`。
需要输密码的命令请在另一个终端标签页（`Alt+T`）里执行。
