# PFC + LLC 智能监控上位机 V1.58

这是从原Qt 5.8电池/BMS监控项目迁出的Qt 5.8兼容工程。当前只保留PFC和LLC遥测，不包含电池、BMS、SOC、气瓶、风机或电磁阀数据。

## 当前能力

- Qt 5.8 Widgets、SerialPort、Network和Charts工程；
- Modbus RTU `0x03`读取24个PFC/LLC寄存器；
- CRC、半包、粘包、噪声和超时重试处理；
- PFC母线、LLC输出电压/电流监控曲线；
- 本地故障与遥测超时判断；
- MiMo V2.5 JSON结构化分析；
- 固定为`advisory_only`：模型返回只显示，不会下发任何控制帧。

## Qt 5.8配置

使用本机已安装的Qt 5.8.0 MSVC2015 64位 Kit，并确认安装以下模块：

- Qt Widgets
- Qt SerialPort
- Qt Network
- Qt Charts
- 与编译器匹配的Desktop Kit（建议64位MSVC或MinGW）

使用Qt Creator打开`FirstProject.pro`，选择Qt 5.8 MSVC2015 64位 Kit后构建、运行。本副本仅支持qmake，不包含CMake工程。该Kit必须使用MSVC2015（v140）编译器；VS2019不能可靠替代该旧Qt Kit。

## MiMo配置

1. 复制`config/agent.example.json`为`config/agent.json`。
2. 将Token Plan API Key设置为系统环境变量`MIMO_API_KEY`，不要写进JSON或源代码。
3. 重启Qt程序。

Token Plan默认Base URL为：

```text
https://token-plan-cn.xiaomimimo.com/v1
```

模型使用`mimo-v2.5-pro`或`mimo-v2.5`。旧MiMo-V2模型名不可用。

## 安全边界

- PFC/LLC的快速控制和保护始终由DSP完成。
- STM32负责CAN状态机、通信心跳和最终命令验证。
- Qt和MiMo只提供监督、诊断和建议。
- 本地遥测过期、PFC故障或LLC故障时不调用MiMo。
- 当前代码没有启动、清故障、升功率、降额或STOP的自动执行逻辑。

STM32需要实现的24寄存器协议和预留控制区见[协议文档](docs/stm32-pfc-llc-modbus.md)。
