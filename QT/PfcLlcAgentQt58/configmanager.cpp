#include "configmanager.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtMath>

#include <limits>

namespace {

int boundedInt(const QJsonObject &object, const char *key, int fallback, int minimum, int maximum)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isDouble()) {
        return fallback;
    }
    return qBound(minimum, value.toInt(), maximum);
}

double optionalDouble(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    return value.isDouble() ? value.toDouble() : std::numeric_limits<double>::quiet_NaN();
}

} // namespace

ApplicationConfig ConfigManager::defaults()
{
    ApplicationConfig config;
    config.mimo.enabled = !qEnvironmentVariableIsEmpty("MIMO_API_KEY");
    return config;
}

bool ConfigManager::load(const QString &filePath, ApplicationConfig *config, QString *error)
{
    if (config == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral("配置输出指针为空");
        }
        return false;
    }
    *config = defaults();

    QFile file(filePath);
    if (!file.exists()) {
        if (error != nullptr) {
            *error = QStringLiteral("未找到配置文件，使用安全默认值：%1").arg(filePath);
        }
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (error != nullptr) {
            *error = file.errorString();
        }
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error != nullptr) {
            *error = QStringLiteral("配置文件JSON错误：%1").arg(parseError.errorString());
        }
        return false;
    }

    const QJsonObject root = document.object();
    const QJsonObject serial = root.value("serial").toObject();
    config->serial.pollIntervalMs = boundedInt(serial, "poll_interval_ms", config->serial.pollIntervalMs, 100, 5000);
    config->serial.responseTimeoutMs = boundedInt(serial, "response_timeout_ms", config->serial.responseTimeoutMs, 50, 5000);
    config->serial.maxRetries = boundedInt(serial, "max_retries", config->serial.maxRetries, 0, 5);

    const QJsonObject mimo = root.value("mimo").toObject();
    config->mimo.enabled = mimo.value("enabled").toBool(config->mimo.enabled);
    const QUrl configuredUrl(mimo.value("base_url").toString());
    if (configuredUrl.isValid() && configuredUrl.scheme() == QLatin1String("https")) {
        config->mimo.baseUrl = configuredUrl;
        QString path = config->mimo.baseUrl.path();
        while (path.endsWith(QLatin1Char('/'))) {
            path.chop(1);
        }
        config->mimo.baseUrl.setPath(path + QLatin1Char('/'));
    }
    const QString model = mimo.value("model").toString();
    if (model == QLatin1String("mimo-v2.5-pro") || model == QLatin1String("mimo-v2.5")) {
        config->mimo.model = model;
    }
    config->mimo.requestIntervalMs = boundedInt(mimo, "request_interval_ms", config->mimo.requestIntervalMs, 1000, 60000);
    config->mimo.requestTimeoutMs = boundedInt(mimo, "request_timeout_ms", config->mimo.requestTimeoutMs, 1000, 60000);
    config->mimo.maxCompletionTokens = boundedInt(mimo, "max_completion_tokens", config->mimo.maxCompletionTokens, 64, 2048);
    if (mimo.value("temperature").isDouble()) {
        config->mimo.temperature = qBound(0.0, mimo.value("temperature").toDouble(), 1.0);
    }
    if (qEnvironmentVariableIsEmpty("MIMO_API_KEY")) {
        config->mimo.enabled = false;
    }

    const QJsonObject safety = root.value("safety").toObject();
    config->safety.telemetryTimeoutMs = boundedInt(safety, "telemetry_timeout_ms", config->safety.telemetryTimeoutMs, 250, 10000);
    config->safety.pfcTemperatureDerateC = optionalDouble(safety, "pfc_temperature_derate_c");
    config->safety.pfcTemperatureTripC = optionalDouble(safety, "pfc_temperature_trip_c");
    config->safety.llcTemperatureDerateC = optionalDouble(safety, "llc_temperature_derate_c");
    config->safety.llcTemperatureTripC = optionalDouble(safety, "llc_temperature_trip_c");
    config->safety.pfcBusVoltageMinV = optionalDouble(safety, "pfc_bus_voltage_min_v");
    config->safety.pfcBusVoltageMaxV = optionalDouble(safety, "pfc_bus_voltage_max_v");
    config->safety.llcOutputVoltageMaxV = optionalDouble(safety, "llc_output_voltage_max_v");
    config->safety.llcOutputCurrentMaxA = optionalDouble(safety, "llc_output_current_max_a");

    // This release intentionally keeps the system in advisory mode, even if a
    // future configuration file accidentally requests automatic control.
    config->advisoryOnly = true;
    return true;
}
