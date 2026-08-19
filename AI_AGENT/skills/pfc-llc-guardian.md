# PFC/LLC Power Guardian

监控 PFC 与 LLC 电源链路，在用户询问电源状态、故障原因、运行风险，或守护任务报告异常时，读取真实遥测并按“保持、降额、安全停机”策略处理；永远不启动设备、不清除故障、不绕过 STM32/DSP 联锁。

## When to use

- 用户询问 PFC、LLC、母线、输出、电流、温度、故障、降额或停机状态。
- 周期守护任务报告遥测超时、状态变化、故障位变化或 LLC 温度升高。
- STM32 网关或 DSP 上报 `Init/Wait/Rise/Run/Err` 状态变化。
- 用户要求解释一次降额、停机或命令拒绝的原因。

不要用于启动电源、清故障、修改保护阈值、写原始 CAN 帧、写 PWM、绕过 ARM 令牌或硬件联锁。

## Required tool contract

使用 `run_shell` 调用受限的 `pfcctl` 命令。若命令不存在、输出不是合法 JSON 或字段缺失，明确报告“设备工具尚未就绪”，不得猜测状态或声称动作成功。

读取状态：

```text
pfcctl status --json
```

返回值至少包含：

```json
{
  "valid": true,
  "age_ms": 200,
  "pfc": {"state": 3, "fault": 0, "ac_v": 220.0, "bus_v": 380.0},
  "llc": {
    "state": 3,
    "fault": 0,
    "output_enabled": true,
    "output_v": 48.0,
    "output_a": 5.0,
    "mos_temp_c": 55,
    "diode_temp_c": 52
  },
  "gateway": {"command_status": "idle", "reject_reason": "none"}
}
```

允许的执行命令仅有：

```text
pfcctl request-derate --percent 80 --reason <REASON_CODE>
pfcctl safe-stop --reason <REASON_CODE>
```

## State and fault decoding

PFC 与 LLC 状态值相同：`0=Init`、`1=Wait`、`2=Rise`、`3=Run`、`4=Err`。

PFC 故障位：

- `0x0001`：AC 输入欠压。
- `0x0002`：AC 输入过压。
- `0x0004`：硬件过流。
- `0x0008`：母线欠压。

LLC 故障位：

- `0x0001`：输出过压。
- `0x0002`：软件输出过流。
- `0x0004`：LLC 原边硬件过流。
- `0x0008`：输出短路。
- `0x0010`：输出欠压。
- `0x0020`：CAN 通信丢失。
- `0x0040`：散热器过温。

多个故障位可以同时存在；逐位解释，不要只解释最低位。

## Safety invariants

1. DSP 的快速保护和 STM32 的安全联锁优先级永远高于 MiMo、ai_agent、网络和 Qt。
2. 任何 PFC/LLC 故障位非零，或任一状态为 `Err`，只允许请求安全停机并读取反馈。
3. `Init`、`Wait`、`Rise` 都不是稳定运行状态；没有故障时保持并观察，不得主动启动。
4. Agent 不得调用启动、故障清除、ARM、原始设定值、原始 CAN、PWM 或保护阈值命令。
5. Agent 不得拥有或推导 ARM 令牌。
6. 停机命令可以由安全策略发起；启动与故障复位必须由人工在硬件侧完成。
7. 所有动作必须由 STM32 再次校验；命令被拒绝时报告拒绝原因，不重试绕过。

## How to use

1. 调用 `run_shell` 执行 `pfcctl status --json`。
2. 验证 `valid=true`、`age_ms<=1500`，并确认全部必需字段存在且为有限数值。
3. 解码 PFC 和 LLC 状态、所有故障位，先处理最高安全等级事件。
4. 按以下顺序决策：
   - 遥测无效或 `age_ms>1500`：执行 `pfcctl safe-stop --reason TELEMETRY_STALE`。
   - 任一故障位非零：执行安全停机，原因使用 `PFC_FAULT_0xNNNN` 或 `LLC_FAULT_0xNNNN`。
   - 任一状态为 `Err`：执行安全停机，原因使用 `PFC_STATE_ERR` 或 `LLC_STATE_ERR`。
   - LLC MOS 或二极管温度 `>=90°C`：执行安全停机，原因使用 `LLC_OVER_TEMPERATURE`。
   - LLC MOS 或二极管温度在 `80°C` 到 `<90°C`，且两级均为 `Run`、无故障、输出已使能：请求 `80%` 降额，原因使用 `LLC_TEMPERATURE_DERATE`。
   - 状态为 `Init/Wait/Rise` 且无故障：保持，不发送控制命令，说明当前过渡状态。
   - 状态均为 `Run`、无故障且温度低于 `80°C`：保持，仅报告状态。
5. 发送降额或停机后，再次执行 `pfcctl status --json`，确认 `command_status`、`reject_reason`、状态、故障和输出使能反馈。
6. 回复必须区分“MiMo 建议”“STM32 接受/拒绝”“DSP 实际状态”，不得把建议描述成已执行。

## Response format

按以下顺序简洁报告：

1. 结论：正常、观察、已请求降额、已请求安全停机或工具不可用。
2. PFC：状态、全部故障、AC/母线关键值。
3. LLC：状态、全部故障、输出电压/电流、MOS/二极管温度。
4. 动作：命令、STM32 处理结果、DSP 反馈。
5. 安全提示：是否需要人工检查、ARM、复位或断电检修。

## Examples

用户：“现在 PFC 和 LLC 正常吗？”

```text
→ run_shell "pfcctl status --json"
→ 解码状态和全部故障位
→ 无故障且均为 Run：只报告，不发送控制命令
```

守护任务：“LLC MOS 温度 85°C，当前无故障。”

```text
→ run_shell "pfcctl status --json"
→ 确认遥测新鲜、PFC/LLC 均为 Run、故障位均为 0
→ run_shell "pfcctl request-derate --percent 80 --reason LLC_TEMPERATURE_DERATE"
→ run_shell "pfcctl status --json"
→ 报告 STM32 是否接受以及 DSP 实际反馈
```

守护任务：“LLC fault=0x000C。”

```text
→ 解码为原边硬件过流 0x0004 + 输出短路 0x0008
→ run_shell "pfcctl safe-stop --reason LLC_FAULT_0x000C"
→ 再次读取状态并要求人工断电检查；不得清故障或自动重启
```
