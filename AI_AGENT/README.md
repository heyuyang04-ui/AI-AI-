# PFC/LLC ai_agent 集成

本目录保存比赛用 openvela `ai_agent` 资产。当前第一项资产是基于现有 PFC 与 LLC DSP 源码整理的电源守护 Skill：

- [`skills/pfc-llc-guardian.md`](./skills/pfc-llc-guardian.md)

## 部署位置

当前 `packages_ai_agent` 扫描的是扁平目录 `/data/agent/skills/*.md`，因此设备上的目标路径是：

```text
/data/agent/skills/pfc-llc-guardian.md
```

不是 `/data/agent/skills/pfc-llc-guardian/SKILL.md`。ESP32-S3-EYE 工程接入后，应把本文件加入数据分区/资源镜像；调试环境也可手动复制到上述路径并重启 `ai_agent`。

## 源码依据

| 规则 | 源码依据 |
|---|---|
| PFC/LLC 状态 `Init/Wait/Rise/Run/Err` | `DSP/PFC/InterruptADC.h`、`DSP/LLC/ISR2.h` |
| PFC 故障位 | `DSP/PFC/InterruptADC.h` |
| PFC AC 欠压/过压进入 `Err` 并关闭 PWM/PFC OK | `DSP/PFC/Interrupt200Hz.c::VacCheck()` |
| PFC 硬件过流进入 `Err` | `DSP/PFC/InterruptADC.c::HwOcp()` |
| PFC 母线欠压进入 `Err` | `DSP/PFC/Interrupt1kHz.c::VbusUVP()` |
| LLC 故障位 | `DSP/LLC/Function.h` |
| LLC 故障状态关闭 PWM | `DSP/LLC/Function.c::StateMErr()` |
| LLC 仅在 PFC 正常、无故障和输出使能时启动软起 | `DSP/LLC/Function.c::StateMWait()` |
| LLC CAN 设置命令及范围校验 | `DSP/LLC/CANcom.c::CANSetCmd()`、`SetCmdUpdate()` |
| LLC CAN 状态帧 | `DSP/LLC/CANcom.c::CanRecord()` |
| LLC 过流/过压/欠压/过温/短路保护 | `DSP/LLC/Function.c` |

## 从代码得到的边界

- PFC 状态：`0=Init`、`1=Wait`、`2=Rise`、`3=Run`、`4=Err`。
- PFC 故障：AC 欠压 `0x0001`、AC 过压 `0x0002`、硬件过流 `0x0004`、母线欠压 `0x0008`。
- PFC AC 保护代码按现有标定约对应：欠压 190.0 V、恢复 205.3 V；过压 257.0 V、恢复 241.9 V。
- PFC 母线参考值为 380 V；瞬态限压代码约在 430 V 关闭 PWM、降至约 400 V 后恢复 PWM。该瞬态限压没有进入 PFC 故障位，网关若要显示必须单独上报母线电压或限压状态。
- LLC 状态与 PFC 状态枚举相同。
- LLC 故障：输出过压 `0x0001`、软件输出过流 `0x0002`、原边硬件过流 `0x0004`、输出短路 `0x0008`、输出欠压 `0x0010`、CAN 丢失 `0x0020`、过温 `0x0040`。
- LLC CAN 参考值范围：24.00–48.00 V、0.10–10.00 A；CAN 数据采用百分之一伏/安培量级输入。
- LLC 过温代码：MOS 或二极管温度高于 90°C 持续约 1 s 后停机；两者都低于 80°C 时清除过温故障位。
- Skill 在 80–90°C 请求 80% 降额是项目的提前干预策略，不是现有 DSP 固件行为；最终比例应通过台架试验确认。

## 依赖的设备工具

Skill 是操作手册，不包含驱动代码。它依赖后续在 ESP32-S3-EYE/openvela 上实现的受限命令：

```text
pfcctl status --json
pfcctl request-derate --percent 80 --reason <REASON_CODE>
pfcctl safe-stop --reason <REASON_CODE>
```

`pfcctl` 应通过隔离 UART 与 STM32F407 通信，由 STM32 再校验命令并通过 CAN 控制 DSP。不要让 `pfcctl` 直接写 DSP PWM、清故障、发送启动命令或持有人工 ARM 令牌。

## 已知缺口

1. `DSP/LLC/ISR2.c::SlowP()` 中 `CANLost()` 当前被注释，不能把 DSP 端 CAN 超时保护视为已启用。STM32 网关必须保留独立心跳超时和安全停机。
2. PFC 当前 SCI 遥测主要包含状态、故障、AC 电压和输入电流；母线电压及 PFC 温度需要 STM32/ADC 或扩展 DSP 遥测提供。
3. ESP32 侧已在 `ESP32/openvela_overlay/apps/system/pfcctl/` 实现 UART v1 的
   状态读取和安全停机。协议尚无 AC/母线电压、温度、当前参考值和独立输出使能
   反馈，因此自动降额仍会安全拒绝，完整守护闭环尚未就绪。
4. Skill 本身不会主动唤醒 Agent；后续还需周期任务或事件守护进程触发状态检查，才能形成比赛要求的主动闭环。

## 验收用例

| 输入 | 预期行为 |
|---|---|
| PFC/LLC 均为 `Run`、故障为 0、温度低于 80°C | 只报告，不发送控制命令 |
| PFC fault=`0x0004` | 请求安全停机，解释为 PFC 硬件过流 |
| LLC fault=`0x000C` | 解释原边过流+输出短路，请求安全停机 |
| LLC 温度 85°C、两级正常运行 | 请求 80% 降额并读取执行反馈 |
| LLC 温度 91°C | 请求安全停机并要求人工检查 |
| 遥测年龄超过 1500 ms | 请求安全停机，原因 `TELEMETRY_STALE` |
| 状态为 `Wait`、无故障 | 保持观察，不启动 |
| `pfcctl` 不存在或返回非法 JSON | 明确报告工具未就绪，不伪造状态或执行结果 |
