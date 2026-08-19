#include "agentdecision.h"

#include <QJsonArray>
#include <QtMath>

#include <limits>

namespace {

AgentAction actionFromString(const QString &value)
{
    if (value == QLatin1String("hold")) {
        return AgentAction::Hold;
    }
    if (value == QLatin1String("derate")) {
        return AgentAction::Derate;
    }
    if (value == QLatin1String("stop")) {
        return AgentAction::Stop;
    }
    if (value == QLatin1String("insufficient_data")) {
        return AgentAction::InsufficientData;
    }
    return AgentAction::Invalid;
}

bool isRiskLevel(const QString &value)
{
    return value == QLatin1String("normal") || value == QLatin1String("warning")
           || value == QLatin1String("critical") || value == QLatin1String("insufficient_data");
}

} // namespace

AgentDecision AgentDecision::fromJson(const QJsonObject &object)
{
    AgentDecision decision;
    const QStringList required{"snapshot_sequence", "risk_level", "action", "suggested_voltage_v",
                               "suggested_current_a", "reason_codes", "explanation", "confidence", "valid_for_ms"};
    for (const QString &field : required) {
        if (!object.contains(field)) {
            decision.error = QStringLiteral("Agent响应缺少字段：%1").arg(field);
            return decision;
        }
    }

    const QJsonValue sequenceValue = object.value("snapshot_sequence");
    const QJsonValue voltageValue = object.value("suggested_voltage_v");
    const QJsonValue currentValue = object.value("suggested_current_a");
    const QJsonValue confidenceValue = object.value("confidence");
    const QJsonValue validForValue = object.value("valid_for_ms");
    if (!sequenceValue.isDouble() || !voltageValue.isDouble() || !currentValue.isDouble()
        || !confidenceValue.isDouble() || !validForValue.isDouble()) {
        decision.error = QStringLiteral("Agent数值字段类型错误");
        return decision;
    }

    const int sequence = sequenceValue.toInt(-1);
    if (sequence < 0 || sequence > std::numeric_limits<quint16>::max()) {
        decision.error = QStringLiteral("Agent snapshot_sequence超出范围");
        return decision;
    }

    decision.snapshotSequence = static_cast<quint16>(sequence);
    decision.riskLevel = object.value("risk_level").toString();
    decision.action = actionFromString(object.value("action").toString());
    decision.suggestedVoltageV = voltageValue.toDouble(std::numeric_limits<double>::quiet_NaN());
    decision.suggestedCurrentA = currentValue.toDouble(std::numeric_limits<double>::quiet_NaN());
    decision.explanation = object.value("explanation").toString();
    decision.confidence = confidenceValue.toDouble(std::numeric_limits<double>::quiet_NaN());
    decision.validForMs = validForValue.toInt(-1);
    for (const QJsonValue &value : object.value("reason_codes").toArray()) {
        if (!value.isString()) {
            decision.error = QStringLiteral("reason_codes必须全部为字符串");
            return decision;
        }
        decision.reasonCodes.append(value.toString());
    }

    if (!isRiskLevel(decision.riskLevel) || decision.action == AgentAction::Invalid
        || !qIsFinite(decision.suggestedVoltageV) || !qIsFinite(decision.suggestedCurrentA)
        || !qIsFinite(decision.confidence) || decision.confidence < 0.0 || decision.confidence > 1.0
        || decision.validForMs < 100 || decision.validForMs > 30000 || decision.explanation.isEmpty()) {
        decision.error = QStringLiteral("Agent响应不符合受限JSON协议");
        return decision;
    }
    decision.valid = true;
    return decision;
}

QString agentActionToString(AgentAction action)
{
    switch (action) {
    case AgentAction::Hold:
        return QStringLiteral("保持建议");
    case AgentAction::Derate:
        return QStringLiteral("降额建议");
    case AgentAction::Stop:
        return QStringLiteral("停止建议");
    case AgentAction::InsufficientData:
        return QStringLiteral("数据不足");
    case AgentAction::Invalid:
        return QStringLiteral("无效建议");
    }
    return QStringLiteral("无效建议");
}
