# AI Studio 主机：硬件清单与风扇控制

**记录日期：** 2026-11-11
**系统环境：** Ubuntu 26.04.1 LTS, Kernel 7.0, GNOME 50 (Wayland)
**用途：** 本地 AI 算力主机（双 RX 7900 XTX，48 GB 显存）

---

## 一、硬件清单

| 部件 | 型号 / 参数 |
|---|---|
| 主板 | 华南金牌 HUANANZHI X99-TF (V7.0)，BIOS 5.11 |
| CPU | Intel Xeon E5-2696 v3：18C/36T，2.3 GHz（睿频 3.8 GHz），L3 45 MB；Haswell-EP，有 AVX2，**无 AVX-512 / AMX** |
| 内存 | 4 × 32 GB DDR4 ECC RDIMM = **128 GB**，四通道插满 |
| GPU | **2 × AMD Radeon RX 7900 XTX 24 GB**（Navi 31 / gfx1100，Sapphire，涡轮版），合计 **48 GB 显存** |
| GPU 链路 | 每卡 PCIe 3.0 x16 直连 CPU（卡支持 4.0，X99 平台上限 3.0） |
| 硬盘 | PHILIPS FM60 2TB NVMe，PCIe 3.0 x4，YEESTOR GMT1000 主控（**无 DRAM 缓存**） |
| 网卡 | Realtek RTL8111 千兆 |
| 传感器芯片 | Nuvoton **NCT6798**（驱动 `nct6775`） |

### 查看命令（无需 sudo）

```bash
lscpu                                   # CPU
free -h                                 # 内存
lspci | grep -Ei 'vga|3d'               # GPU
cat /sys/class/drm/card*/device/mem_info_vram_total   # 显存（字节）
lsblk -d -o NAME,MODEL,SIZE,TRAN        # 硬盘
sensors                                 # 温度、风扇（需 lm-sensors）
sudo dmidecode -t memory                # 内存条频率 / 型号（需 sudo）
```

> 内核 EDAC 会把内存类型报成 "Registered-DDR3"，是 `sb_edac` 驱动的标注问题，E5 v3 只支持 DDR4。

### 对本地大模型的影响

- 48 GB 显存：约可完整放下 70B 的 4-bit 量化，或 30B 级别的 8-bit 量化；llama.cpp 可把模型按层切到两张卡。
- 卡间只能走 PCIe 3.0（每卡约 16 GB/s），无 NVLink 类直连：张量并行受限，按层切分影响小。
- CPU 无 AVX-512，CPU offload 推理较慢；128 GB 内存够放，但速度上不去。
- ROCm 尚未安装（`amdgpu` 驱动已加载，用户对 `/dev/kfd` 有 ACL 读写权限）。

---

## 二、风扇噪音排查结论

空闲时（CPU 30～40°C，GPU 23°C）风扇也一直转：

| 风扇 | 空闲转速 | 判断 |
|---|---|---|
| GPU 0 涡轮风扇 | ~2850 rpm（40% PWM，最高 6000） | **主要噪音来源** |
| GPU 1 涡轮风扇 | ~2880 rpm | **主要噪音来源** |
| 主板 fan1（CPU） | ~1500 rpm | 正常 |
| 主板 fan2（机箱） | ~1370 rpm | 正常 |

涡轮（鼓风式）版 7900 XTX 默认关闭空闲停转，且转速上限高，是为多卡并排设计的代价。
**BIOS 只能调主板风扇，管不了显卡风扇**，所以要在系统里控制。

---

## 三、配置步骤

### 3.1 加载主板传感器驱动（看到 CPU / 机箱风扇）

脚本：[load_motherboard_sensors.sh](../scripts/load_motherboard_sensors.sh)

```bash
bash Linux/ubuntu/scripts/load_motherboard_sensors.sh
```

依次尝试 `nct6775` / `it87` / `w83627ehf` / `f71882fg`，识别成功后写入
`/etc/modules-load.d/sensors.conf` 开机自动加载。本机结果：`nct6775` → `nct6798-isa-0a20`。

### 3.2 开放 7900 XTX 风扇控制接口（内核参数）

RDNA3 显卡的风扇曲线要通过驱动的 overdrive 接口 `gpu_od/fan_ctrl/` 设置，默认关闭。
不修改 `/etc/default/grub`，而是放一个独立的 drop-in 文件，撤销时删掉即可：

`/etc/default/grub.d/amdgpu-fan-control.cfg`：

```bash
# Only adds bit 0x4000 (PP_OVERDRIVE_MASK) to the driver's default mask 0xfff7bfff.
GRUB_CMDLINE_LINUX_DEFAULT="$GRUB_CMDLINE_LINUX_DEFAULT amdgpu.ppfeaturemask=0xfff7ffff"
```

```bash
sudo update-grub && sudo reboot
```

> 网上常见写法是 `0xffffffff`，会顺带打开其他默认关闭的功能。这里只在默认掩码
> （`cat /sys/module/amdgpu/parameters/ppfeaturemask`）上加 `0x4000` 一位。

验证：

```bash
grep -o 'amdgpu.ppfeaturemask=[^ ]*' /proc/cmdline
ls /sys/bus/pci/devices/0000:05:00.0/gpu_od/fan_ctrl/
# fan_curve  fan_zero_rpm_enable  fan_zero_rpm_stop_temperature  ...
```

撤销：

```bash
sudo rm /etc/default/grub.d/amdgpu-fan-control.cfg && sudo update-grub && sudo reboot
```

### 3.3 图形化控制：CoolerControl（一个软件全包）

```bash
curl -1sLf 'https://dl.cloudsmith.io/public/coolercontrol/coolercontrol/setup.deb.sh' | sudo -E bash
sudo apt install -y coolercontrol
sudo systemctl enable --now coolercontrold
```

推荐显卡曲线起点：50°C 以下 20～25%（或开启 zero RPM 空闲停转），70°C 50%，85°C 以上 100%。

---

## 四、工具对比

| 工具 | 看什么 | 能调吗 | 本机是否需要 |
|---|---|---|---|
| Resources（系统自带） | CPU / 内存 / GPU 占用、进程 | 只能结束进程 | 自带 |
| Disk Usage Analyzer（自带） | 文件夹占用空间 | 否 | 自带 |
| Sysprof（自带） | 程序性能剖析（函数级 CPU 时间） | 否 | 自带 |
| Psensor | 温度、风扇曲线图 | 否 | 不需要 |
| LACT | AMD 显卡负载、功耗、风扇曲线 | 是 | 不需要 |
| **CoolerControl** | 所有温度 + 风扇 | **是，主板 + 显卡统一** | ✅ 唯一需要装的 |

---

## 五、顺带发现

- `clash-verge` 空闲时占约 80% 单核 CPU，属异常，可重启该程序。
- SYSTIN 读数 10°C，探头无效，忽略。
