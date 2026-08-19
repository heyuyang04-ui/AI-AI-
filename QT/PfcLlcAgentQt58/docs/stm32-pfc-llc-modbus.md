# STM32—Qt PFC/LLC Modbus 协议 V1

本协议替代原先电池、BMS、气瓶和电磁阀遥测定义。STM32作为Modbus RTU从机，地址固定为`0x09`，串口参数为115200、8N1。CRC采用标准Modbus CRC16，低字节先发送。

## 只读遥测区

Qt每500ms发送：

```text
09 03 00 00 00 18 CRC_L CRC_H
```

`0x0000`开始连续24个保持寄存器：

| 地址 | 名称 | 类型/比例 |
|---:|---|---|
| 0 | protocol_version | `1` |
| 1 | system_state | 状态机枚举 |
| 2 | telemetry_sequence | 每次发布遥测加一 |
| 3 | pfc_state | DSP状态 |
| 4 | pfc_fault | PFC故障位图 |
| 5 | pfc_ac_input_voltage | 0.1 V |
| 6 | pfc_bus_voltage | 0.1 V |
| 7 | pfc_input_current | 0.1 A |
| 8 | pfc_temperature | 0.01 ℃ |
| 9 | llc_state | DSP状态 |
| 10 | llc_fault | LLC故障位图 |
| 11 | llc_input_voltage | 0.1 V |
| 12 | llc_input_current | 0.1 A |
| 13 | llc_output_voltage | 0.1 V |
| 14 | llc_output_current | 0.1 A |
| 15 | llc_temperature | 0.01 ℃ |
| 16 | llc_output_enable | `0`禁止，`1`使能 |
| 17 | command_status | 命令处理结果 |
| 18 | command_reject_reason | 拒绝原因枚举 |
| 19 | active_target_voltage | 0.1 V |
| 20 | active_target_current | 0.1 A |
| 21 | host_heartbeat_age_10ms | 主机心跳距今时间，单位10ms |
| 22 | reserved | 固定0 |
| 23 | reserved | 固定0 |

PFC、LLC任何故障位非零时，STM32必须保持或进入安全状态。Qt只显示故障；DSP快速保护不得依赖Qt或网络。

## 预留控制区

Qt V1.60是只建议模式，绝不调用此接口。该控制区为后续人工确认模式预留。

使用Modbus `0x10`写6个连续寄存器，必须一次性写完整帧：

| 地址 | 名称 | 说明 |
|---:|---|---|
| 0x0100 | command_sequence | 单调递增序号 |
| 0x0101 | action | `0`保持、`1`降额、`2`停止、`3`启动 |
| 0x0102 | target_voltage | 0.1V |
| 0x0103 | target_current | 0.1A |
| 0x0104 | arm_token | 人工ARM令牌 |
| 0x0105 | protocol_version | 固定`1` |

STM32处理要求：

1. 先收集完整`0x10`帧，再执行；禁止按寄存器逐项立即生效。
2. 校验序号、协议版本、目标范围、DSP/PFC状态、硬件联锁和ARM令牌。
3. STOP可立即处理；START只能由人工ARM后的有效令牌触发。
4. Agent不得拥有ARM令牌，也不得请求清故障。
5. 将成功、拒绝和拒绝原因写回地址17、18。
6. 主机心跳超时后的安全动作由STM32和DSP决定，不能依赖Qt继续在线。

## 迁移注意

当前旧STM32工程的`Func_06()`按地址直接写`g_SysStatus`内存。该行为不得沿用到本协议。控制命令必须写入单独的命令结构，经完整验证后再提交到PFC/LLC CAN状态机。
