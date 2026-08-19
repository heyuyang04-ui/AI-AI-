#pragma once

#include <QJsonObject>
#include <QStringList>

enum class AgentAction {
    Hold,
    Derate,
    Stop,
    InsufficientData,
    Invalid
};

struct AgentDecision
{
    bool valid = false;
    quint16 snapshotSequence = 0;
    QString riskLevel;
    AgentAction action = AgentAction::Invalid;
    double suggestedVoltageV = 0.0;
    double suggestedCurrentA = 0.0;
    QStringList reasonCodes;
    QString explanation;
    double confidence = 0.0;
    int validForMs = 0;
    QString error;

    static AgentDecision fromJson(const QJsonObject &object);
};

QString agentActionToString(AgentAction action);
