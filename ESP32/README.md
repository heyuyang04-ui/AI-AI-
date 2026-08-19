# ESP32-S3-EYE openvela 侧

本目录提供 ESP32-S3-EYE 与 STM32F407 PFC/LLC 网关之间的第一版
openvela/NuttX 应用。ESP32 通过 UART1 使用现有 `A5 5A` 二进制协议，不直接
访问 DSP、CAN、PWM 或保护参数。

## 硬件连接

| ESP32-S3-EYE | STM32F407 | 说明 |
|---|---|---|
| IO45 / UART1 TX | PC7 / USART6 RX | ESP32 发送到 STM32 |
| IO46 / UART1 RX | PC6 / USART6 TX | STM32 发送到 ESP32 |
| GND | GND | 仅限同一安全低压域；正式样机建议数字隔离 |

两端均为 `3.3 V` IO，通信参数为 `115200-8-N-1`。IO45/IO46 是启动相关
管脚，复位期间外部电路不得强拉。高压 PFC/LLC 样机建议使用双通道数字隔离器
和两侧隔离电源。

## 文件映射

将以下目录复制或链接到 openvela 的应用树：

```text
openvela_overlay/apps/system/pfcctl -> apps/system/pfcctl
```

将 `configs/esp32s3-eye-pfcctl.config` 的配置项合并到
`vendor/espressif/boards/esp32s3/esp32s3-eye/configs/openvela/defconfig`，
再重新配置并构建：

```bash
./build.sh vendor/espressif/boards/esp32s3/esp32s3-eye/configs/openvela/ -j8
```

USB Serial 作为控制台时，UART1 通常注册为 `/dev/ttyS0`。如果实际设备节点
不同，在配置中修改 `CONFIG_SYSTEM_PFCCTL_DEVICE`，不要改源码。

## 命令

```text
pfcctl status --json
pfcctl safe-stop --reason MANUAL_TEST
pfcctl request-derate --percent 80 --reason LLC_TEMPERATURE_DERATE
```

- `status` 请求 STM32 立即返回 v1 状态帧并输出 JSON。
- `safe-stop` 发送 `enable=0`，等待 STM32 ACK/REJECT，然后再次读取状态。
- `request-derate` 当前会安全拒绝。v1 协议没有“当前参考值”、温度和独立的
  `output_enabled` 字段，并且一次性 CLI 无法持续发送控制心跳；据此直接发送
  `enable=1` 存在误启动风险。

## 当前协议限制

STM32 的 UART v1 状态只有 PFC/LLC 状态、故障、输出电压和输出电流。因此
JSON 中 AC 电压、母线电压、温度和独立输出使能状态为 `null`，并带有
`telemetry_complete=false`。现有 `pfc-llc-guardian` Skill 会把这些缺失字段
视为设备工具尚未完全就绪，不会猜测温度或自动降额。

下一版应先扩展 STM32/DSP 遥测和协议，再增加常驻 `pfcctld` 维护心跳；在这两项
完成前，不应把一次性 `pfcctl` 扩展成启机或降额控制器。

## 主机测试

协议编解码不依赖 openvela，可在仓库根目录执行：

```bash
gcc -std=c11 -Wall -Wextra -Werror \
  ESP32/openvela_overlay/apps/system/pfcctl/pfcctl_protocol.c \
  ESP32/tests/test_protocol.c -o ESP32/tests/test_protocol
./ESP32/tests/test_protocol
```
