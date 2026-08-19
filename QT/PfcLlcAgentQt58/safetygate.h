#pragma once

#include "agentdecision.h"
#include "localruleengine.h"

struct FinalDecision
{
    LocalAction localAction = LocalAction::Hold;
    AgentAction agentAction = AgentAction::Invalid;
    bool commandPermitted = false;
    QString summary;
};

class SafetyGate
{
public:
    FinalDecision evaluate(const RuleDecision &local, const AgentDecision *agent) const;
};
