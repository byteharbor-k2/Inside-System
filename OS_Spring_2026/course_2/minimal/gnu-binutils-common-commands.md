# GNU Binutils 常见命令用法

这篇文档只讲 **GNU Binutils** 里的常见命令。

要先澄清一个边界：

- `readelf`、`objdump`、`strings`、`nm`、`size` 属于 GNU Binutils
- `file` 不属于 Binutils
- `xxd` 也不属于 Binutils

`file` 和 `xxd` 只是经常和 Binutils 一起使用，所以很容易被误以为是一套工具。

本文仍然以当前目录里的文件为例：

- `minimal.o`：目标文件
- `minimal`：最终 ELF 可执行文件

## 一句话理解

- `readelf`：按 ELF 结构看文件
- `objdump`：看反汇编、section、符号等更底层的信息
- `strings`：快速提取文件里的可打印字符串
- `nm`：看符号表
- `size`：看代码段、数据段的大致体积

## 1. `readelf`

`readelf` 适合“按 ELF 规范读文件”，不做太多额外推断。

常用命令：

```bash
readelf -h minimal
readelf -l minimal
readelf -S minimal
readelf -s minimal
```

常见用途：

- `-h`：看 ELF Header
- `-l`：看 Program Header，接近内核加载视角
- `-S`：看 Section Header，接近链接器视角
- `-s`：看符号表

对于当前 `minimal`，最重要的信息通常是：

- 文件是 `ELF64`
- 类型是 `EXEC`
- 架构是 `x86-64`
- 入口地址是 `0x401000`

什么时候优先用它：

- 你想知道“这是不是真正的 ELF”
- 你想看入口地址
- 你想看内核会映射哪些段
- 你想区分 section 和 segment

## 2. `objdump`

`objdump` 更像一个“多功能二进制观察台”，最常用的是反汇编。

常用命令：

```bash
objdump -d minimal
objdump -d minimal.o
objdump -h minimal
objdump -t minimal
```

常见用途：

- `-d`：反汇编可执行代码
- `-h`：看 section 摘要
- `-t`：看符号表

对 `minimal` 来说，`objdump -d minimal` 能直接看到：

- `_start` 在什么地址
- 每条机器码对应什么汇编指令
- 程序最终到底执行了哪些指令

什么时候优先用它：

- 你想看“CPU 最终执行什么”
- 你想对照源码和机器码
- 你想确认编译器/汇编器到底生成了什么

注意：

- `objdump -d` 有时会把数据也尝试按指令反汇编
- 所以它很强，但不能脱离上下文生搬硬看

## 3. `strings`

`strings` 很适合做第一轮快速摸底。

命令：

```bash
strings minimal
strings minimal.o
```

它会把文件中的可打印字符串提取出来。

对 ELF 来说，它常用于快速看到：

- 路径
- 符号名
- 报错消息
- 嵌入的文本内容

什么时候优先用它：

- 你不知道文件里有什么，想先扫一眼
- 你怀疑某个字符串被编进去了
- 你想快速找线索，而不是先看完整 ELF 结构

## 4. `nm`

`nm` 用来看符号表，适合回答“这个目标文件里定义了哪些符号、引用了哪些符号”。

命令：

```bash
nm minimal.o
nm minimal
```

常见用途：

- 看函数符号
- 看全局变量符号
- 看哪些符号是未定义、等待链接的

对 `.o` 文件尤其有用，因为它还处在“待链接”阶段。

什么时候优先用它：

- 你想知道某个符号到底有没有被定义
- 你想排查链接错误
- 你想看 `_start`、`main`、全局变量这些名字是否真的进了目标文件

## 5. `size`

`size` 很简单，但很实用。

命令：

```bash
size minimal
size minimal.o
```

它会告诉你：

- `.text` 大小
- `.data` 大小
- `.bss` 大小

什么时候优先用它：

- 你想快速比较两个程序谁更大
- 你在做嵌入式或最小化程序时，想看体积变化

## 6. 一组常见的组合用法

如果你拿到一个 ELF 文件，常见顺序可以是：

```bash
readelf -h minimal
readelf -l minimal
objdump -d minimal
strings minimal
size minimal
```

如果你拿到的是 `.o` 文件，常见顺序可以是：

```bash
readelf -h minimal.o
readelf -S minimal.o
nm minimal.o
objdump -d minimal.o
size minimal.o
```

## 7. 和其他常用工具的关系

在实际工作里，GNU Binutils 往往和别的工具一起使用：

- `file`：先判断文件类型
- `xxd`：直接看十六进制字节
- `hexdump` / `od`：看原始字节
- `grep` / `rg`：在文本输出里继续筛信息

这些工具经常和 Binutils 同时出现，但它们不一定属于 GNU Binutils 本身。
