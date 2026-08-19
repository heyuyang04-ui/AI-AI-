#pragma once

#include <QByteArray>
#include <QVector>

namespace PfcLlcProtocol {

constexpr quint8 kSlaveAddress = 0x09;
constexpr quint8 kReadHoldingRegisters = 0x03;
constexpr quint8 kWriteMultipleRegisters = 0x10;
constexpr quint16 kTelemetryBaseAddress = 0x0000;
constexpr quint16 kControlBaseAddress = 0x0100;

enum class TelemetryRegister : quint16 {
    ProtocolVersion = 0,
    SystemState,
    TelemetrySequence,
    PfcState,
    PfcFault,
    PfcAcInputVoltage,
    PfcBusVoltage,
    PfcInputCurrent,
    PfcTemperature,
    LlcState,
    LlcFault,
    LlcInputVoltage,
    LlcInputCurrent,
    LlcOutputVoltage,
    LlcOutputCurrent,
    LlcTemperature,
    LlcOutputEnable,
    CommandStatus,
    CommandRejectReason,
    ActiveTargetVoltage,
    ActiveTargetCurrent,
    HostHeartbeatAge10ms,
    Reserved22,
    Reserved23
};

constexpr int kTelemetryRegisterCount = 24;

enum class ControlRegister : quint16 {
    CommandSequence = kControlBaseAddress,
    Action,
    TargetVoltageDeciVolt,
    TargetCurrentDeciAmp,
    ArmToken,
    ProtocolVersion
};

enum class CommandAction : quint16 {
    Hold = 0,
    Derate = 1,
    Stop = 2,
    Start = 3
};

struct Frame {
    quint8 address = 0;
    quint8 function = 0;
    QByteArray payload;
    QByteArray raw;
};

quint16 crc16(const QByteArray &bytes);
QByteArray buildReadHolding(quint16 firstRegister, quint16 registerCount);
QByteArray buildWriteMultiple(quint16 firstRegister, const QVector<quint16> &values);
bool verifyFrame(const QByteArray &raw);
bool decodeReadHolding(const Frame &frame, QVector<quint16> *registers, QString *error = nullptr);
bool decodeWriteMultipleAck(const Frame &frame, quint16 *firstRegister, quint16 *count,
                            QString *error = nullptr);

} // namespace PfcLlcProtocol
