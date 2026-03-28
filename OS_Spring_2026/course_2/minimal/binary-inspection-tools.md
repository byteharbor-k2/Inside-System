# 二进制文件查看方法总结

本文以当前目录下的这几个文件为例：

- `minimal.S`：带预处理的汇编源文件
- `minimal.s`：预处理后的纯汇编文本
- `minimal.o`：目标文件（relocatable ELF）
- `minimal`：最终可执行文件（executable ELF）

## 一句话理解

- `file`：先判断“这是什么类型的文件”
- `xxd`：直接看原始字节
- `readelf`：按 ELF 结构解析
- `objdump`：按机器码和反汇编视角解析

## 1. `file`

最适合做第一步判断。

命令：

```bash
file minimal minimal.o minimal.s minimal.S
```

当前目录中的典型结果：

```text
minimal:   ELF 64-bit LSB executable, x86-64, version 1 (SYSV), statically linked, not stripped
minimal.o: ELF 64-bit LSB relocatable, x86-64, version 1 (SYSV), not stripped
minimal.s: assembler source, ASCII text
minimal.S: C source, Unicode text, UTF-8 text
```

怎么看：

- `minimal` 是最终可执行 ELF，内核可以加载它
- `minimal.o` 是可重定位目标文件，给链接器用，内核通常不会直接执行它
- `minimal.s`、`minimal.S` 都是文本文件

从 OS 视角：

- `file` 不关心程序逻辑，只帮你识别“这个字节流属于什么格式”
- 它适合回答“这是文本、ELF、共享库、压缩包，还是别的什么”

## 2. `xxd`

最适合看“文件的原始字节到底长什么样”。

命令：

```bash
xxd -g 1 -l 96 minimal
```

当前目录中的典型结果：

```text
00000000: 7f 45 4c 46 02 01 01 00 00 00 00 00 00 00 00 00  .ELF............
00000010: 02 00 3e 00 01 00 00 00 00 10 40 00 00 00 00 00  ..>.......@.....
00000020: 40 00 00 00 00 00 00 00 60 11 00 00 00 00 00 00  @.......`.......
00000030: 00 00 00 00 40 00 38 00 02 00 40 00 05 00 04 00  ....@.8...@.....
```

怎么看：

- 开头 `7f 45 4c 46` 就是 ELF 魔数
- 右侧是把部分字节按可打印字符显示后的辅助视图
- 大部分内容对人不直观，但它说明：所谓“二进制文件”本质上也是一串字节

从 OS 视角：

- `xxd` 不解释格式，只原样展示字节
- 它适合验证魔数、查看头部、确认某段字符串或某几个字节是否真的存在

## 3. `readelf`

最适合按 ELF 规范去理解文件结构。

常用命令：

```bash
readelf -h minimal
readelf -l minimal
readelf -S minimal
```

当前目录里最关键的两个命令是：

```bash
readelf -h minimal
readelf -l minimal
```

你会看到类似信息：

```text
ELF Header:
  Class:                             ELF64
  Type:                              EXEC (Executable file)
  Machine:                           Advanced Micro Devices X86-64
  Entry point address:               0x401000
```

```text
Program Headers:
  Type           Offset             VirtAddr
  LOAD           0x0000000000000000 0x0000000000400000
  LOAD           0x0000000000001000 0x0000000000401000
```

怎么看：

- `Type: EXEC` 表示这是可执行文件
- `Machine: X86-64` 表示目标架构
- `Entry point address: 0x401000` 表示进程启动后第一条指令地址
- `Program Headers` 表示内核加载 ELF 时要把哪些内容映射到虚拟内存、权限是什么

从 OS 视角：

- `readelf -h` 让你看 ELF header，也就是文件总说明书
- `readelf -l` 让你看 program header，这是内核加载进程时最关心的部分
- `readelf -S` 看 section，更偏链接器和调试工具视角，不是内核加载时的核心视角

## 4. `objdump`

最适合从“机器码到底会执行什么指令”这个角度看文件。

命令：

```bash
objdump -d minimal
```

当前目录中的关键输出类似这样：

```text
0000000000401000 <_start>:
  401000: 48 c7 c0 01 00 00 00  mov    $0x1,%rax
  401007: 48 c7 c7 01 00 00 00  mov    $0x1,%rdi
  40100e: 48 8d 35 19 00 00 00  lea    0x19(%rip),%rsi
  401015: 48 c7 c2 1c 00 00 00  mov    $0x1c,%rdx
  40101c: 0f 05                 syscall
```

怎么看：

- 这里已经不是源码，而是最终机器码对应的汇编反汇编
- 可以直接看到 `_start` 的地址和每条指令
- 也能看到 `syscall`，说明程序确实是直接发系统调用

从 OS 视角：

- `objdump -d` 最适合回答“CPU 最终会执行什么”
- 它常用于核对入口点、查看函数机器码、对比源码和最终指令

注意：

- `objdump -d` 有时也会把数据区域误当成指令继续反汇编，所以不能无脑相信每一行
- 反汇编结果要结合符号和 section 一起看

## 什么时候用哪个

- 只想先知道文件类型：`file`
- 想看原始字节、魔数、十六进制内容：`xxd`
- 想理解 ELF 头、入口地址、段映射：`readelf`
- 想看 CPU 最终执行的指令：`objdump`

## 对 `minimal/` 这个例子的推荐顺序

```bash
file minimal
xxd -g 1 -l 64 minimal
readelf -h minimal
readelf -l minimal
objdump -d minimal
```

这五步基本能把一个最小 ELF 程序从“文件类型”到“原始字节”再到“OS 如何加载”和“CPU 如何执行”串起来。
