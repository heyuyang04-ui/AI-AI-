#include "mimoclient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>

MimoClient::MimoClient(QObject *parent)
    : QObject(parent)
{
    m_timeoutTimer.setParent(this);
    m_timeoutTimer.setSingleShot(true);
    connect(&m_timeoutTimer, &QTimer::timeout, this, [this] {
        if (m_reply != nullptr) {
            m_timedOut = true;
            m_reply->abort();
        }
    });
}

void MimoClient::setConfig(const MimoConfig &config)
{
    m_config = config;
    if (!isEnabled()) {
        emit serviceStateChanged(QStringLiteral("Agent未启用：未配置MIMO_API_KEY或配置关闭"));
    }
}

bool MimoClient::isEnabled() const
{
    return m_config.enabled && !qEnvironmentVariableIsEmpty("MIMO_API_KEY") && m_config.baseUrl.isValid();
}

bool MimoClient::isBusy() const
{
    return m_reply != nullptr;
}

bool MimoClient::analyze(const TelemetrySnapshot &snapshot, const QJsonObject &trend)
{
    if (!isEnabled() || isBusy() || !snapshot.valid) {
        return false;
    }

    const QByteArray apiKey = qgetenv("MIMO_API_KEY");
    QUrl endpoint = m_config.baseUrl;
    endpoint.setPath(endpoint.path() + QStringLiteral("chat/completions"));

    QNetworkRequest request(endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("api-key", apiKey);

    m_timedOut = false;
    m_reply = m_network.post(request, QJsonDocument(makeRequest(snapshot, trend)).toJson(QJsonDocument::Compact));
    m_timeoutTimer.start(m_config.requestTimeoutMs);
    emit serviceStateChanged(QStringLiteral("MiMo分析请求中"));

    connect(m_reply, &QNetworkReply::finished, this, &MimoClient::finishReply);
    return true;
}

QJsonObject MimoClient::makeRequest(const TelemetrySnapshot &snapshot, const QJsonObject &trend) const
{
    const QString systemPrompt = QStringLiteral(
        "你是PFC和LLC电源系统的运行分析助手。只返回一个合法JSON对象，不能输出Markdown或额外解释。"
        "你只能给出建议，不能绕过本地保护，不能清除故障，不能要求启动系统。"
        "action只能是hold、derate、stop、insufficient_data。"
        "返回结构必须严格为："
        "{\"snapshot_sequence\":number,\"risk_level\":\"normal|warning|critical|insufficient_data\","
        "\"action\":\"hold|derate|stop|insufficient_data\",\"suggested_voltage_v\":number,"
        "\"suggested_current_a\":number,\"reason_codes\":[string],\"explanation\":string,"
        "\"confidence\":number,\"valid_for_ms\":number}。"
        "若数据无效、故障存在或不足，action必须是insufficient_data或stop。"
        "不得臆测未提供的参数。"
    );

    QJsonObject userPayload{
        {"telemetry", snapshot.toJson()},
        {"trend", trend},
        {"control_mode", QStringLiteral("advisory_only")}
    };
    return {
        {"model", m_config.model},
        {"messages", QJsonArray{
            QJsonObject{{"role", "system"}, {"content", systemPrompt}},
            QJsonObject{{"role", "user"},
                        {"content", QString::fromUtf8(QJsonDocument(userPayload).toJson(QJsonDocument::Compact))}}
        }},
        {"response_format", QJsonObject{{"type", "json_object"}}},
        {"max_completion_tokens", m_config.maxCompletionTokens},
        {"temperature", m_config.temperature},
        {"stream", false}
    };
}

void MimoClient::finishReply()
{
    if (m_reply == nullptr) {
        return;
    }
    m_timeoutTimer.stop();
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;

    const QByteArray body = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError networkError = reply->error();
    const QString networkErrorText = reply->errorString();
    reply->deleteLater();

    if (m_timedOut) {
        emit serviceError(QStringLiteral("MiMo请求超时"));
        return;
    }
    if (networkError != QNetworkReply::NoError) {
        emit serviceError(QStringLiteral("MiMo网络错误(%1)：%2").arg(status).arg(networkErrorText));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        emit serviceError(QStringLiteral("MiMo响应JSON无效：%1").arg(parseError.errorString()));
        return;
    }
    const QJsonArray choices = document.object().value("choices").toArray();
    if (choices.isEmpty()) {
        emit serviceError(QStringLiteral("MiMo响应未包含choices"));
        return;
    }
    const QString content = choices.first().toObject().value("message").toObject().value("content").toString();
    const QJsonDocument decisionDocument = QJsonDocument::fromJson(content.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !decisionDocument.isObject()) {
        emit serviceError(QStringLiteral("MiMo未返回受限JSON：%1").arg(parseError.errorString()));
        return;
    }
    const AgentDecision decision = AgentDecision::fromJson(decisionDocument.object());
    if (!decision.valid) {
        emit serviceError(decision.error);
        return;
    }
    emit serviceStateChanged(QStringLiteral("MiMo分析完成"));
    emit decisionReady(decision);
}
