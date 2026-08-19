#pragma once

#include "telemetrysnapshot.h"

#include <limits>

struct SafetyConfig
{
    int telemetryTimeoutMs = 1500;
    double pfcTemperatureDerateC = std::numeric_limits<double>::quiet_NaN();
    double pfcTemperatureTripC = std::numeric_limits<double>::quiet_NaN();
    double llcTemperatureDerateC = std::numeric_limits<double>::quiet_NaN();
    double llcTemperatureTripC = std::numeric_limits<double>::quiet_NaN();
    double pfcBusVoltageMinV = std::numeric_limits<double>::quiet_NaN();
    double pfcBusVoltageMaxV = std::numeric_limits<double>::quiet_NaN();
    double llcOutputVoltageMaxV = std::numeric_limits<double>::quiet_NaN();
    double llcOutputCurrentMaxA = std::numeric_limits<double>::quiet_NaN();
};

enum class LocalAction {
    Stop,
    Derate,
    Hold,
    Allow
};

struct RuleDecision
{
    LocalAction action = LocalAction::Hold;
    QStringList reasonCodes;
    QString summary;
};

class LocalRuleEngine
{
public:
    void setConfig(const SafetyConfig &config);
    RuleDecision evaluate(const TelemetrySnapshot &snapshot) const;

private:
    SafetyConfig m_config;
};

QString localActionToString(LocalAction action);
