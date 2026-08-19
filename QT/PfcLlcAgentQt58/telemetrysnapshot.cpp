#include "telemetrysnapshot.h"

#include "modbusprotocol.h"

#include <QtGlobal>

#include <limits>

int TelemetrySnapshot::ageMs() const
{
    if (!timestamp.isValid()) {
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(timestamp.msecsTo(QDateTime::currentDateTimeUtc()));
}

double TelemetrySnapshot::llcOutputPowerW() const
{
    return llcOutputVoltageV * llcOutputCurrentA;
}

QJsonObject TelemetrySnapshot::toJson() const
{
    return {
        {"snapshot_sequence", static_cast<int>(sequence)},
        {"data_age_ms", ageMs()},
        {"valid", valid},
        {"system", QJsonObject{
            {"state", static_cast<int>(systemState)},
            {"command_status", static_cast<int>(commandStatus)},
            {"command_reject_reason", static_cast<int>(commandRejectReason)},
            {"host_heartbeat_age_ms", static_cast<int>(hostHeartbeatAge10ms) * 10}
        }},
        {"pfc", QJsonObject{
            {"state", static_cast<int>(pfcState)},
            {"fault", static_cast<int>(pfcFault)},
            {"ac_input_voltage_v", pfcAcInputVoltageV},
            {"bus_voltage_v", pfcBusVoltageV},
            {"input_current_a", pfcInputCurrentA},
            {"temperature_c", pfcTemperatureC}
        }},
        {"llc", QJsonObject{
            {"state", static_cast<int>(llcState)},
            {"fault", static_cast<int>(llcFault)},
            {"input_voltage_v", llcInputVoltageV},
            {"input_current_a", llcInputCurrentA},
            {"output_voltage_v", llcOutputVoltageV},
            {"output_current_a", llcOutputCurrentA},
            {"temperature_c", llcTemperatureC},
            {"output_enabled", llcOutputEnabled},
            {"output_power_w", llcOutputPowerW()},
            {"active_target_voltage_v", activeTargetVoltageV},
            {"active_target_current_a", activeTargetCurrentA}
        }}
    };
}

bool TelemetrySnapshot::fromRegisters(const QVector<quint16> &registers,
                                      TelemetrySnapshot *snapshot,
                                      QString *error)
{
    if (snapshot == nullptr || registers.size() != PfcLlcProtocol::kTelemetryRegisterCount) {
        if (error != nullptr) {
            *error = QStringLiteral("遥测寄存器数量错误：期望 %1，收到 %2")
                         .arg(PfcLlcProtocol::kTelemetryRegisterCount)
                         .arg(registers.size());
        }
        return false;
    }

    const auto value = [&registers](PfcLlcProtocol::TelemetryRegister index) {
        return registers.at(static_cast<int>(index));
    };

    TelemetrySnapshot result;
    result.timestamp = QDateTime::currentDateTimeUtc();
    result.valid = true;
    result.protocolVersion = value(PfcLlcProtocol::TelemetryRegister::ProtocolVersion);
    result.systemState = value(PfcLlcProtocol::TelemetryRegister::SystemState);
    result.sequence = value(PfcLlcProtocol::TelemetryRegister::TelemetrySequence);
    result.pfcState = value(PfcLlcProtocol::TelemetryRegister::PfcState);
    result.pfcFault = value(PfcLlcProtocol::TelemetryRegister::PfcFault);
    result.pfcAcInputVoltageV = value(PfcLlcProtocol::TelemetryRegister::PfcAcInputVoltage) / 10.0;
    result.pfcBusVoltageV = value(PfcLlcProtocol::TelemetryRegister::PfcBusVoltage) / 10.0;
    result.pfcInputCurrentA = value(PfcLlcProtocol::TelemetryRegister::PfcInputCurrent) / 10.0;
    result.pfcTemperatureC = value(PfcLlcProtocol::TelemetryRegister::PfcTemperature) / 100.0;
    result.llcState = value(PfcLlcProtocol::TelemetryRegister::LlcState);
    result.llcFault = value(PfcLlcProtocol::TelemetryRegister::LlcFault);
    result.llcInputVoltageV = value(PfcLlcProtocol::TelemetryRegister::LlcInputVoltage) / 10.0;
    result.llcInputCurrentA = value(PfcLlcProtocol::TelemetryRegister::LlcInputCurrent) / 10.0;
    result.llcOutputVoltageV = value(PfcLlcProtocol::TelemetryRegister::LlcOutputVoltage) / 10.0;
    result.llcOutputCurrentA = value(PfcLlcProtocol::TelemetryRegister::LlcOutputCurrent) / 10.0;
    result.llcTemperatureC = value(PfcLlcProtocol::TelemetryRegister::LlcTemperature) / 100.0;
    result.llcOutputEnabled = value(PfcLlcProtocol::TelemetryRegister::LlcOutputEnable) != 0;
    result.commandStatus = value(PfcLlcProtocol::TelemetryRegister::CommandStatus);
    result.commandRejectReason = value(PfcLlcProtocol::TelemetryRegister::CommandRejectReason);
    result.activeTargetVoltageV = value(PfcLlcProtocol::TelemetryRegister::ActiveTargetVoltage) / 10.0;
    result.activeTargetCurrentA = value(PfcLlcProtocol::TelemetryRegister::ActiveTargetCurrent) / 10.0;
    result.hostHeartbeatAge10ms = value(PfcLlcProtocol::TelemetryRegister::HostHeartbeatAge10ms);
    *snapshot = result;
    return true;
}
