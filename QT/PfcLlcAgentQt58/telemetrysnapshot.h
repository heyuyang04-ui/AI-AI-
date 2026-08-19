#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QVector>

struct TelemetrySnapshot
{
    QDateTime timestamp;
    quint16 sequence = 0;
    bool valid = false;

    quint16 protocolVersion = 0;
    quint16 systemState = 0;

    quint16 pfcState = 0;
    quint16 pfcFault = 0;
    double pfcAcInputVoltageV = 0.0;
    double pfcBusVoltageV = 0.0;
    double pfcInputCurrentA = 0.0;
    double pfcTemperatureC = 0.0;

    quint16 llcState = 0;
    quint16 llcFault = 0;
    double llcInputVoltageV = 0.0;
    double llcInputCurrentA = 0.0;
    double llcOutputVoltageV = 0.0;
    double llcOutputCurrentA = 0.0;
    double llcTemperatureC = 0.0;
    bool llcOutputEnabled = false;

    quint16 commandStatus = 0;
    quint16 commandRejectReason = 0;
    double activeTargetVoltageV = 0.0;
    double activeTargetCurrentA = 0.0;
    quint16 hostHeartbeatAge10ms = 0;

    int ageMs() const;
    double llcOutputPowerW() const;
    QJsonObject toJson() const;

    static bool fromRegisters(const QVector<quint16> &registers,
                              TelemetrySnapshot *snapshot,
                              QString *error = nullptr);
};
