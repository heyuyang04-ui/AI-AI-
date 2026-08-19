#pragma once

#include "agentdecision.h"
#include "configmanager.h"
#include "telemetrysnapshot.h"

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTimer>

class MimoClient : public QObject
{
    Q_OBJECT

public:
    explicit MimoClient(QObject *parent = nullptr);

    void setConfig(const MimoConfig &config);
    bool isEnabled() const;
    bool isBusy() const;
    bool analyze(const TelemetrySnapshot &snapshot, const QJsonObject &trend);

signals:
    void decisionReady(const AgentDecision &decision);
    void serviceStateChanged(const QString &state);
    void serviceError(const QString &message);

private:
    QJsonObject makeRequest(const TelemetrySnapshot &snapshot, const QJsonObject &trend) const;
    void finishReply();

    QNetworkAccessManager m_network;
    QTimer m_timeoutTimer;
    MimoConfig m_config;
    QPointer<QNetworkReply> m_reply;
    bool m_timedOut = false;
};
