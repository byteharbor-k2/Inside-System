# 常见文件查看命令与工具归属

这篇文档专门讲那些**经常用来查看文件内容**、但**不属于 GNU Binutils 本体**的常见工具。

很多人会把它们混成一类，是因为平时排查文件、读代码、看二进制时，它们经常一起出现。

## 1. 先说结论

这些常见命令并不都属于 `GNU coreutils`。

比较常见的归属是：

- `cat`、`head`、`tail`：通常属于 `GNU coreutils`
- `objdump`、`readelf`、`strings`：属于 `GNU Binutils`
- `file`：属于 `file` / `libmagic` 这一套
- `xxd`：通常来自 `vim-common`
- `sed`：属于 GNU `sed`
- `awk`：通常是 `gawk` 或 `mawk`
- `rg`：是 `ripgrep`
- `less`：是 `less`

所以：

> “常见查看文件命令”是一个使用场景分类，不是一个统一的软件包分类。

## 2. `file`

`file` 用来判断“这是什么文件”。

示例：

```bash
file minimal
file minimal.o
file hello.c
```

它背后主要依赖的是 `libmagic` 规则库，通过文件头、魔数和模式来识别格式。

它不属于 `GNU coreutils`，也不属于 `GNU Binutils`。

适合场景：

- 先判断文件类型
- 分辨文本、ELF、压缩包、图片、共享库

## 3. `xxd`

`xxd` 用来看十六进制和原始字节。

示例：

```bash
xxd -g 1 -l 64 minimal
xxd hello.c | head
```

它不属于 `GNU coreutils`，通常随 `vim-common` 一起提供。

适合场景：

- 看文件头魔数
- 看原始字节
- 验证某一段内容是否真的写进文件

## 4. `cat`、`head`、`tail`

这几个是最典型的 `GNU coreutils` 工具。

示例：

```bash
cat hello.c
head -n 20 minimal.S
tail -n 20 README.md
```

适合场景：

- 快速查看文本
- 取开头几行、结尾几行
- 配合管道继续处理

注意：

- 它们主要适合文本
- 对纯二进制直接 `cat` 往往没有太大意义，还可能把终端刷乱

## 5. `sed`

`sed` 是流编辑器，不属于 `coreutils`，而是 GNU `sed` 这个单独项目。

示例：

```bash
sed -n '1,40p' hello.c
sed -n '1,80p' README.md
```

适合场景：

- 按行打印
- 做简单替换
- 非交互式处理文本文件

## 6. `awk`

`awk` 是文本处理语言和命令，常见实现是 `gawk` 或 `mawk`。

它也不属于 `coreutils`。

示例：

```bash
awk '{print NR \": \" $0}' hello.c
```

适合场景：

- 按列处理文本
- 快速写小型文本分析逻辑
- 处理日志、表格化输出

## 7. `rg`

`rg` 是 `ripgrep`，不是 `coreutils`，也不是 `binutils`。

示例：

```bash
rg "_start" .
rg "printf" OS_Spring_2026/course_2
```

适合场景：

- 快速全文检索
- 在代码仓库中找符号、字符串、配置项

## 8. 面向“看文件”的实用分组

如果按使用目的来分，比按软件包名更有用：

- 看文件类型：`file`
- 看纯文本：`cat`、`head`、`tail`、`sed`
- 查文本内容：`rg`、`awk`
- 看原始字节：`xxd`
- 看 ELF 和目标文件：`readelf`、`objdump`、`nm`、`strings`

## 9. 一个更实用的认知方式

不要把这些命令死记成“都属于哪一个大包”，而要先记住：

- 它们解决的是什么问题
- 它们最适合看什么文件
- 它们输出的是文本、结构信息，还是原始字节

一个简单记法：

- `coreutils` 更像通用基础命令
- `binutils` 更像编译后世界的工具
- `file` / `xxd` / `sed` / `awk` / `rg` 是其他常见配套工具
