#include "safetygate.h"

FinalDecision SafetyGate::evaluate(const RuleDecision &local, const AgentDecision *agent) const
{
    FinalDecision result;
    result.localAction = local.action;
    result.agentAction = agent == nullptr ? AgentAction::Invalid : agent->action;

    if (local.action == LocalAction::Stop) {
        result.summary = QStringLiteral("本地规则要求停止；Agent建议已被忽略");
        return result;
    }
    if (local.action == LocalAction::Derate) {
        result.summary = QStringLiteral("本地规则要求降额；当前版本仅显示建议，不发送控制命令");
        return result;
    }
    if (agent == nullptr || !agent->valid) {
        result.summary = QStringLiteral("无有效Agent建议，保持本地监控");
        return result;
    }
    result.summary = QStringLiteral("Agent建议已通过格式校验；当前为只建议模式，未下发控制命令");
    return result;
}
