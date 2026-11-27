# checkm8-a5 on Arduino Uno — Apple TV 3,1 / iPhone 4S

把 `checkm8` 的 A5 移植（[synackuk/checkm8-a5](https://github.com/synackuk/checkm8-a5)）跑在一块
Arduino Uno + MAX3421E USB Host Shield 上，用来把 A5 设备打进 **pwned DFU**。

本文档记录的是**在这台机器、这块板、这几台设备上实际发生过什么**——包括成功的、失败的、以及被
我自己误判过的东西。所有结论后面都尽量附上当时的日志片段作为证据。

---

## 1. 这是什么，不是什么

**是**：一个跑在 Arduino 上的漏洞利用固件。它通过 USB 主机模式对处于 DFU 的 A5 设备执行
checkm8，成功后设备序列号里会出现 `PWND:[checkM8]`。

**不是**：

- 不是 Apple TV 的固件，不安装任何系统；
- 不是越狱本身。对 ATV 3,1 来说，pwned DFU 是 [Blackb0x](https://github.com/NSSpiral/Blackb0x)
  越狱的**第 0 步**（原文："0. (3,1 only) PWN with Arduino + synackuk's fork of checkm8-A5"）；
- 不会持久化。**断电即失效**，每次使用都要重跑。

checkm8 对 A5 的作用是 **demote**：打开 SWD 调试、让设备用 development KBAG 解密 Image3。
它不等于能启动未签名系统。

---

## 2. 已验证的结果

| 设备 | SoC / 标识 | 结果 | 证据 |
|---|---|---|---|
| **iPhone 4S** | `CPID:8940` `SRTG:[IBOOT-838.3]` | ✅ pwn 成功（两次） | 本机 PnP：`..._PWND:[CHECKM8]` |
| **Apple TV 3,1** | `CPID:8942` | ✅ pwn 成功 | macOS「系统信息 → USB」显示 pwned DFU |
| SanDisk U 盘 | `VID_0x781 PID_0x5581` | ✅ 稳定枚举 | 用于验证 shield 的 A 口与 VBUS |
| iPad 3 | `S5L8945` | ⚠️ 未验证 | 只有编译分支，没有实机 |

iPhone 4S 的完整设备 ID（判据以**主机重新枚举**为准）：

```
USB\VID_05AC&PID_1227\CPID:8940_CPRV:21_CPFM:03_SCEP:01_BDID:08_\
ECID:0000XXXXXXXXXXXXXXXX_IBFL:00_SRTG:[IBOOT-838.3]_PWND:[CHECKM8]
```

---

## 3. 快速上手（推荐路径）

1. 烧 **原版** 对应 SoC 的 hex（见 §7）：
   - iPhone 4S / iPad 2 → `checkm8-a5-A5_8940-PRISTINE-uno.hex`
   - Apple TV 3,1 → `checkm8-a5-ATV3-1-PRISTINE-uno.hex`
2. 设备接 **shield 的 USB-A**（ATV 必须**先接 shield 再上电**，见 §8）；
3. 让它进入 DFU；
4. 串口出现 `1. heap feng-shui` → `Done!`；
5. **把设备挪到电脑上**，看设备 ID 里有没有 `PWND:[CHECKM8]`（见 §9）。

不要用带心跳/verify 的诊断版去跑 exploit，原因见 §10.1。

---

## 4. 硬件

### 4.1 接线

```
电脑 ──USB──> Arduino Uno R3 ──叠板──> USB Host Shield (MAX3421E) ──> 设备
```

- Uno R3：`VID_2341&PID_0043`，串口在 `COM28` / `COM29`（换 USB 口会变号）；
- Shield：MAX3421E，`rREVISION = 0x13`；
- 设备 micro-USB 接 shield 的 **USB-A 母口**。

### 4.2 LED（可选）

`D6 → 220–470 Ω → LED → GND`。

> 上游文档只写 "LED (must be connected to pin 6)"，**没有电阻**，作者照片里也是直连。
> 5 V 直驱 2.0–2.2 V LED 会到 30 mA 以上，超过 ATmega328P 单脚 40 mA 的绝对上限，务必串电阻。
>
> 另外原版 sketch **没有** `pinMode(6, OUTPUT)`，`digitalWrite` 只切换内部上拉（约 0.1 mA），
> 所以说明书里那句"闪 3 次"实际上几乎看不见。这是上游文档缺陷。

### 4.3 这台机器上 shield 的实际状态（重要）

这块 shield 的 **5 V 焊盘已经焊坏**，目前用一根飞线把 **3.3 V** 引到 USB-A 的 VBUS（1 脚）。

后果：

- U 盘这类 ~100 mA 的总线供电设备勉强能枚举；
- iPhone 一插就抖（3.3 V 欠压，且 iPhone 想充电）；
- Apple TV 影响较小——它是市电供电，不从 VBUS 取电，只需要 D+ 上拉和 3.3 V 信号电平。

**建议修法**：飞线改接 Uno 的 **5V 针**（不是 3.3V），并在中间串 500 mA 保险丝；
VBUS 处补回 `4.7 µF` 电容。若 Uno 由 USB 供电（500 mA 上限），iPhone 充电时仍会拖垮，
更稳的做法是 VBUS 单独用 5 V/2 A 电源供电、与 Uno 共地。

### 4.4 稳定性实测

修好之前，**一小时内 USB 掉线 34 次**（平均不到 2 分钟一次，最密集时几秒三次）。

```
06:16:33 serial lost
06:16:54 serial lost
06:17:31 serial lost
06:17:33 serial lost
...
```

在这种状态下跑 exploit 基本靠运气：有一次跑到一半板子掉了，日志出现第二次 `checkm8 started`。
**先修供电，再谈成功率。**

---

## 5. 构建

### 5.1 为什么不用 arduino-cli

`downloads.arduino.cc` 在这台机器上 TLS 握手即被重置（HTTP 同样），`arduino-cli` 和官方 AVR core
都下不动。改用 **PlatformIO**：平台与工具链走 `dl.registry.platformio.org`，正常可达。

```powershell
python -m platformio run -d orig8940 -t upload --upload-port COM29
python -m platformio run -d pio -e ipad2 -t upload      # 诊断版（不推荐用于实战）
```

### 5.2 USB Host 库补丁（必须）

`usb_host_library.patch` 共 4 处，必须应用，否则检测不到设备：

1. `Usb.cpp`：新增 `USB::ctrlReq_SETUP()`（发送不带数据/状态阶段的 SETUP）；
2. `Usb.cpp` 的 `Task()`：`Configuring(0,0,lowspeed)` → `rcode = 0`
   （**关键**：跳过标准枚举，设备才能留在地址 0）；
3. `UsbCore.h`：`USB_XFER_TIMEOUT` 5000 → **12 ms**；
4. `UsbCore.h`：增加 `ctrlReq_SETUP` 声明。

> 上游 README 特别注明：A5 版的补丁与 `DSecurity/checkm8-arduino` 那份**不同**，不能混用。

### 5.3 Payload 与 SoC 选择

| 来源 | 大小/SoC | 说明 |
|---|---|---|
| synackuk（原版） | 641 B | `orig8940/`、`orig8942/` 使用，**4S 实机验证通过** |
| a1exdandy | 705 B | `pio/`、`a1ex/` 使用，ATV 首次成功即此版 |

两个仓库的 `.ino`、库补丁、宏、`padding` 完全一致，**只有 `constants.h` 的 payload 数据不同**。

`padding`：`8942 → 0x640`，`8940 / 8945 → 0x6c0`。

SoC 由 sketch 顶部条件编译选择（默认 `A5_8942`，`-DA5_8940` 切换），编译时会打印
`Building for A5 S5L8942X` 之类的 pragma 便于确认。

---

## 6. 工程清单

| 目录 | 用途 | 用于实战？ |
|---|---|---|
| `orig8940/` | **纯原版** sketch + 原版 641 B payload，`#define A5_8940` | ✅ 推荐 |
| `orig8942/` | 同上，`#define A5_8942` | ✅ 推荐 |
| `pio/` | 带诊断的版本（LED 自检、心跳、abort、verify），`-e uno` / `-e ipad2` | ❌ 只用于调试 |
| `a1ex/` | `pio/` 的诊断版 + a1ex 705 B payload | ❌ |
| `auto/` | 单固件运行时读 CPID 自动选 payload（8940/8942/8945） | ⚠️ 实验性，见 §10.5 |
| `probe/` | 只读描述符，打印状态机、VID/PID、raw 字节 | 诊断 |
| `lister/` | 端口探测器：任何设备都报 VID/PID + 总线 SE0/J/K | 诊断 |
| `regs/` | 直接读 MAX3421E 寄存器（revision / USBIRQ / HIRQ / HRSL） | 诊断 |
| `stock/` | 未打补丁的库 + 读地址 1，用于证明"地址 0/1"问题 | 诊断 |
| `serial_r/` | 读 DFU 字符串描述符（CPID / ECID / 序列号） | 诊断 |

辅助脚本：`apply_patch.py`（无 git 环境的 unified-diff 应用）、`gen_payloads.py`（生成多 SoC
`payloads.h`）、`serial_log.py`（断线自动重连的串口日志器）、`read_serial.py`、`open-log.cmd`。

---

## 7. 产物（hex）

| 文件 | 大小 | 说明 |
|---|---|---|
| `checkm8-a5-A5_8940-PRISTINE-uno.hex` | 7792 B | **iPhone 4S 实测 pwn 成功** |
| `checkm8-a5-ATV3-1-PRISTINE-uno.hex` | 7794 B | Apple TV 3,1 用 |
| `checkm8-a5-A5_8940-uno.hex` | 8948 B | 诊断版（8940） |
| `checkm8-a5-ATV3-1-uno.hex` | 9016 B | 诊断版（8942） |
| `checkm8-a5-A5_8940-a1ex-uno.hex` | 8594 B | a1ex payload + 诊断 |
| `checkm8-a5-AUTO-uno.hex` | 11268 B | 多 SoC 自动选择（实验性） |

SHA-256：

```
checkm8-a5-A5_8940-PRISTINE-uno.hex   0ecae32306b9fffcb108a71ab6edd40f4f7a3662fad8e3729973e6cbb908f20a
checkm8-a5-ATV3-1-PRISTINE-uno.hex    99959df64afeaf4b01f64038d2937ba91ef0feb925be492c9646c62859533ac7
checkm8-a5-A5_8940-uno.hex            d95a25976e43f992831749ac73870a5c46ded83fb657bf1f4922a3ca5563545e
checkm8-a5-ATV3-1-uno.hex             d3417740affb84c5712497276433fdfbb38fe8c251586d9e8482a4ced2fe1bf6
checkm8-a5-A5_8940-a1ex-uno.hex       0e61315c92ba712060885492c8e102768c827fac94f1bd2b9bc9bba89858e237
checkm8-a5-AUTO-uno.hex               a59dfe3b22248368e4e83967d18afaa4f58f2716f32fda43d1578f6bc548b8c8
```

烧写校验（典型输出）：

```
avrdude: Device signature = 0x1e950f (probably m328p)
avrdude: 7792 bytes of flash written
avrdude: 7792 bytes of flash verified
```

---

## 8. 进 DFU

### 8.1 iPhone 4S

DFU 靠 bootrom，跟系统无关：电源+Home 10 秒 → 松电源、继续按住 Home 5–8 秒 → 屏幕全黑。
4S 有电池，**换 USB 线不会退出 DFU**（只要不断电）。

### 8.2 Apple TV 3,1（关键差异）

**DFU 必须在 shield 已经连接的情况下进入。** 这是整件事最大的坑：

| 做法 | 结果 |
|---|---|
| 先在电脑上进 DFU，再拔线换到 shield | 设备已退出 DFU，shield 只看到 `0x12A7`（正常）或 `0x1281`（恢复） |
| **先接 shield → 断电 → 按住 Menu+Play → 上电 → 继续按 7 秒** | 正确，盾上直接出现 `0x1227` 并开始利用 |

ATV 的三种 USB 形态：

| 状态 | PID |
|---|---|
| 正常模式 | `0x12A7` |
| 恢复模式 | `0x1281` |
| **DFU（可利用）** | `0x1227` |

固件在非 `0x1227` 时会打印 `Non Apple DFU found (vendorId: 1452, productId: ...)`（十进制），
这一行本身就证明 shield 与设备之间的链路是通的。

---

## 9. 怎么判断成功（唯一可靠的方法）

**把设备挪到电脑上，看设备 ID 是否以 `_PWND:[CHECKM8]` 结尾。**

- ✅ `USB\VID_05AC&PID_1227\CPID:8940_..._PWND:[CHECKM8]`
- ❌ 干净的 `CPID:8940...`（没有 PWND）= 没成功，但设备健康
- ❌ `Unknown USB Device (Invalid Configuration Descriptor)` = 描述符被写坏，需强制重启设备

在 shield 上"跑完后立刻读字符串描述符"是**不可靠**的：读到的常常是半更新的乱码
（`CPID:...SCEP:?CPID:...`）或空串、甚至是不存在的索引（48、52）。据此判断成败会得出完全错误的结论
——我在这上面误判过一次，写在这里当反面教材。

---

## 10. 踩过的坑（每条都有证据）

### 10.1 诊断代码会干扰 exploit

同一台 4S、同一块板、同一根线：

| 固件 | 结果 |
|---|---|
| 纯原版（`orig8940`） | **pwn 成功** |
| 带心跳 / `bus=` 寄存器读 / abort / verify 的版本 | `Done!` 后描述符被写坏，多次复现失败 |

原因：心跳在 `checkm8_state != CHECKM8_END` 期间每 5 秒执行一次，而 `heap_feng_shui` 的 620 次
控制传输约需 8 秒——**心跳正好落在 exploit 窗口内**，`regRd(rHRSL)` + 打印在时序中间插了一刀。

结论：**exploit 路径上不要加任何诊断 I/O**。诊断要么放在检测之前，要么放在 exploit 结束之后。
（注：ATV 那次成功用的恰好是带诊断的版本，说明这不是必然失败，而是**概率被显著拉低**。）

### 10.2 地址 0 与地址 1

未打补丁的库会在枚举末尾调用 `Configuring()` → `DefaultAddressing()` → `setAddr(0,0,1)`，
把设备从地址 0 挪到 1。而 sketch 在地址 0 读描述符，于是永远读不到、`desc_buf` 全零，
打印 `Non Apple DFU found (vendorId: 0, productId: 0)`。

A/B 实测（`stock/` vs `probe/`）：

```
[stock] getDevDescr(addr=0) rc=0xD
[stock] getDevDescr(addr=1) rc=0x0     → VID=0x5AC PID=0x1227
[probe] getDevDescr try 1 rc=0x0       → 地址 0 直接可读
```

这就是补丁第 2 条的由来。

### 10.3 "没有 VBUS" —— 误判

U 盘插上毫无反应，据此判断 A 口没供电。后来 U 盘稳定枚举到 `RUNNING`，推翻。

### 10.4 "iPhone 取电把 5 V 拉垮" —— 部分误判

attach 后 1–2 秒掉线，归因于大电流。实际当时的主因是 **USB-A 连接处虚焊**——按住就能稳定 50 秒以上。

教训：**先排查机械接触，再怀疑供电与代码。**

### 10.5 "Done!" 不等于成功

`heap_occupation()` 返回后无条件打印 `Done!`，它只代表函数走完了。真正的判据见 §9。

### 10.6 自动识别版（`auto/`）

为了不用为每个 SoC 单独编译，做了运行时读 `CPID:` 选 payload 的版本。它确实能正确打印

```
CPID read: 0x8940
target: S5L8940 (iPhone 4S / iPad 2)
```

但那次运行最终没 pwn（`verify` 读到空串）。由于它比原版多了一次字符串描述符读取，
**不能排除这次额外 control transfer 扰动了 DFU 堆**。在拿到更多样本之前，它只能算实验性。

### 10.7 寄存器读数别读错位

`rHRSL` 的 **bit7 = JSTATUS / bit6 = KSTATUS**，低 4 位是 HRSLT 结果码。

- `0x83` = **J 态**（有全速设备），不是 SE1；
- `0xC0` 才是 SE1（D+/D- 都高，通常意味着线悬空）。

我第一次把它读成 SE1，导致方向跑偏。

---

## 11. 未解决 / 下一步

1. **shield 供电**：飞线从 3.3 V 改到 5 V，补 VBUS 电容，最好让 VBUS 独立供电（§4.3）。
   这是目前最大的阻塞项——一小时内 34 次掉线，任何 exploit 都跑不稳。
2. **块 shield 换新** 可能比修更省时间。
3. `auto/` 版本需要在稳定供电下重新评估。
4. iPad 3（S5L8945）分支没有任何实机验证。
5. ATV 3,1 的后续越狱（Blackb0x）未在这台机器上跑过，本文档只到 pwned DFU 为止。

---

## 12. 附

### 12.1 常用命令

```powershell
# 烧原版 8940（端口自动换成你的）
python -m platformio run -d orig8940 -t upload --upload-port COM29

# 看实时日志（会随文件增长）
Get-Content .\monitor.log -Wait -Tail 60 -Encoding UTF8

# 诊断：端口探测器
python -m platformio run -d lister -t upload --upload-port COM29
```

### 12.2 Windows Defender 误报

构建期间 Defender 对本机一条命令脚本误报 `Program:Script/Wacapew.A!ml`（ML 启发式），
把派生的 shell 拦掉了（`spawn EPERM`）。属于误报。
建议给本目录加排除项，而不是长期关闭实时保护。

### 12.3 参考

- [synackuk/checkm8-a5](https://github.com/synackuk/checkm8-a5) —— 本项目的代码来源
- [a1exdandy/checkm8-a5](https://github.com/a1exdandy/checkm8-a5) —— 705 B payload 版本
- [NSSpiral/Blackb0x](https://github.com/NSSpiral/Blackb0x) —— ATV 2/3 越狱（3,1 需要先 pwn）
- [Elcomsoft: 如何把 Apple TV 3 置入 DFU](https://blog.elcomsoft.com/2022/10/how-to-put-apple-tv-3-2012-2013-apple-tv-hd-2015-and-apple-tv-4k-2017-into-dfu/)

---

## 13. 作者与贡献

| 角色 | 贡献 |
|---|---|
| **PaperM0**（[@PaperM0](https://github.com/PaperM0)） | 硬件搭建与修复（Uno、USB Host Shield、5 V 焊盘损坏后的飞线）、设备操作（DFU 进入、Blackb0x）、全部实机验证 |
| **DeepSeek Harness AI agent**（deepseek-flash） | 工具链搭建（PlatformIO、USB Host 库补丁）、固件构建与烧写、故障定位（地址 0/1、HRSL 误读、虚焊与供电判别）、诊断固件与自动化脚本、本文档撰写 |

> 说明：AI 助手不主张著作权。代码来自 MIT 许可的上游项目
> （[synackuk/checkm8-a5](https://github.com/synackuk/checkm8-a5)、[a1exdandy/checkm8-a5](https://github.com/a1exdandy/checkm8-a5)），
> 文档与诊断工具由本次协作产生。
运行环境：[DeepSeek](https://github.com/deepseek-ai) 的 DeepSeek Harness（模型 deepseek-v4.1-flash）。
