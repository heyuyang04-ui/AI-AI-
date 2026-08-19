#include "localruleengine.h"

#include <QtMath>

namespace {

bool configured(double value)
{
    return qIsFinite(value);
}

RuleDecision makeDecision(LocalAction action, const QStringList &codes, const QString &summary)
{
    RuleDecision decision;
    decision.action = action;
    decision.reasonCodes = codes;
    decision.summary = summary;
    return decision;
}

RuleDecision stop(const QString &code, const QString &summary)
{
    return makeDecision(LocalAction::Stop, QStringList() << code, summary);
}

} // namespace

void LocalRuleEngine::setConfig(const SafetyConfig &config)
{
    m_config = config;
}

RuleDecision LocalRuleEngine::evaluate(const TelemetrySnapshot &snapshot) const
{
    if (!snapshot.valid || snapshot.ageMs() > m_config.telemetryTimeoutMs) {
        return stop(QStringLiteral("TELEMETRY_STALE"), QStringLiteral("PFC/LLC遥测无效或已超时"));
    }
    if (snapshot.pfcFault != 0) {
        return stop(QStringLiteral("PFC_FAULT"), QStringLiteral("PFC报告故障，禁止继续运行"));
    }
    if (snapshot.llcFault != 0) {
        return stop(QStringLiteral("LLC_FAULT"), QStringLiteral("LLC报告故障，禁止继续运行"));
    }
    if (!qIsFinite(snapshot.pfcBusVoltageV) || !qIsFinite(snapshot.llcOutputVoltageV)
        || !qIsFinite(snapshot.llcOutputCurrentA) || !qIsFinite(snapshot.pfcTemperatureC)
        || !qIsFinite(snapshot.llcTemperatureC)) {
        return stop(QStringLiteral("TELEMETRY_NONFINITE"), QStringLiteral("遥测存在非法数值"));
    }
    if (configured(m_config.pfcTemperatureTripC)
        && snapshot.pfcTemperatureC >= m_config.pfcTemperatureTripC) {
        return stop(QStringLiteral("PFC_OVER_TEMPERATURE"), QStringLiteral("PFC温度达到停机阈值"));
    }
    if (configured(m_config.llcTemperatureTripC)
        && snapshot.llcTemperatureC >= m_config.llcTemperatureTripC) {
        return stop(QStringLiteral("LLC_OVER_TEMPERATURE"), QStringLiteral("LLC温度达到停机阈值"));
    }
    if (configured(m_config.pfcBusVoltageMinV) && snapshot.pfcBusVoltageV < m_config.pfcBusVoltageMinV) {
        return stop(QStringLiteral("PFC_BUS_UNDERVOLTAGE"), QStringLiteral("PFC母线电压低于安全下限"));
    }
    if (configured(m_config.pfcBusVoltageMaxV) && snapshot.pfcBusVoltageV > m_config.pfcBusVoltageMaxV) {
        return stop(QStringLiteral("PFC_BUS_OVERVOLTAGE"), QStringLiteral("PFC母线电压高于安全上限"));
    }
    if (configured(m_config.llcOutputVoltageMaxV) && snapshot.llcOutputVoltageV > m_config.llcOutputVoltageMaxV) {
        return stop(QStringLiteral("LLC_OUTPUT_OVERVOLTAGE"), QStringLiteral("LLC输出电压高于安全上限"));
    }
    if (configured(m_config.llcOutputCurrentMaxA) && snapshot.llcOutputCurrentA > m_config.llcOutputCurrentMaxA) {
        return stop(QStringLiteral("LLC_OUTPUT_OVERCURRENT"), QStringLiteral("LLC输出电流高于安全上限"));
    }

    QStringList derateReasons;
    if (configured(m_config.pfcTemperatureDerateC)
        && snapshot.pfcTemperatureC >= m_config.pfcTemperatureDerateC) {
        derateReasons.append(QStringLiteral("PFC_TEMPERATURE_DERATE"));
    }
    if (configured(m_config.llcTemperatureDerateC)
        && snapshot.llcTemperatureC >= m_config.llcTemperatureDerateC) {
        derateReasons.append(QStringLiteral("LLC_TEMPERATURE_DERATE"));
    }
    if (!derateReasons.isEmpty()) {
        return makeDecision(LocalAction::Derate, derateReasons, QStringLiteral("温度接近限制，建议降额"));
    }
    return makeDecision(LocalAction::Allow, QStringList(), QStringLiteral("本地PFC/LLC安全规则通过"));
}

QString localActionToString(LocalAction action)
{
    switch (action) {
    case LocalAction::Stop:
        return QStringLiteral("停止");
    case LocalAction::Derate:
        return QStringLiteral("降额");
    case LocalAction::Hold:
        return QStringLiteral("保持");
    case LocalAction::Allow:
        return QStringLiteral("允许分析");
    }
    return QStringLiteral("未知");
}
