#pragma once

#include "localruleengine.h"
#include "serialmodbusclient.h"

#include <QUrl>

struct MimoConfig
{
    bool enabled = false;
    QUrl baseUrl = QUrl(QStringLiteral("https://token-plan-cn.xiaomimimo.com/v1/"));
    QString model = QStringLiteral("mimo-v2.5-pro");
    int requestIntervalMs = 5000;
    int requestTimeoutMs = 10000;
    int maxCompletionTokens = 512;
    double temperature = 0.1;
};

struct ApplicationConfig
{
    SerialModbusClient::Settings serial;
    MimoConfig mimo;
    SafetyConfig safety;
    bool advisoryOnly = true;
};

class ConfigManager
{
public:
    static ApplicationConfig defaults();
    static bool load(const QString &filePath, ApplicationConfig *config, QString *error = nullptr);
};
