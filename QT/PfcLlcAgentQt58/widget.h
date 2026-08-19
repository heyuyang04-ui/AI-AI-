#pragma once

#include "agentdecision.h"
#include "configmanager.h"
#include "localruleengine.h"
#include "mimoclient.h"
#include "safetygate.h"
#include "serialmodbusclient.h"

#include <QComboBox>
#include <QFormLayout>
#include <QJsonObject>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>
#include <QWidget>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

QT_CHARTS_USE_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);

private slots:
    void refreshPorts();
    void toggleSerial();
    void analyzeCurrentSnapshot();

private:
    void buildUi();
    void updateTelemetry(const TelemetrySnapshot &snapshot);
    void updateLocalDecision(const RuleDecision &decision);
    void updateAgentDecision(const AgentDecision &decision);
    void updateFinalDecision();
    void appendLog(const QString &message);
    void appendCharts(const TelemetrySnapshot &snapshot);
    QJsonObject trendSummary() const;
    QLabel *addReadOnlyValue(QFormLayout *layout, const QString &caption);
    QChartView *createChart(const QString &title, QLineSeries **series, QValueAxis **axisY);

    ApplicationConfig m_config;
    SerialModbusClient m_serial;
    LocalRuleEngine m_localRules;
    MimoClient m_mimo;
    SafetyGate m_safetyGate;
    QTimer m_agentTimer;

    TelemetrySnapshot m_snapshot;
    RuleDecision m_localDecision;
    AgentDecision m_agentDecision;
    bool m_hasAgentDecision = false;

    QComboBox *m_portBox = nullptr;
    QPushButton *m_openButton = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QPushButton *m_analyzeButton = nullptr;
    QLabel *m_connectionLabel = nullptr;

    QLabel *m_pfcState = nullptr;
    QLabel *m_pfcFault = nullptr;
    QLabel *m_pfcAcInput = nullptr;
    QLabel *m_pfcBusVoltage = nullptr;
    QLabel *m_pfcInputCurrent = nullptr;
    QLabel *m_pfcTemperature = nullptr;

    QLabel *m_llcState = nullptr;
    QLabel *m_llcFault = nullptr;
    QLabel *m_llcInputVoltage = nullptr;
    QLabel *m_llcInputCurrent = nullptr;
    QLabel *m_llcOutputVoltage = nullptr;
    QLabel *m_llcOutputCurrent = nullptr;
    QLabel *m_llcTemperature = nullptr;
    QLabel *m_llcOutputEnable = nullptr;
    QLabel *m_llcPower = nullptr;

    QLabel *m_localRuleLabel = nullptr;
    QLabel *m_agentStateLabel = nullptr;
    QLabel *m_agentDecisionLabel = nullptr;
    QLabel *m_finalDecisionLabel = nullptr;
    QLabel *m_agentExplanationLabel = nullptr;
    QPlainTextEdit *m_logView = nullptr;

    QLineSeries *m_pfcBusSeries = nullptr;
    QLineSeries *m_llcVoltageSeries = nullptr;
    QLineSeries *m_llcCurrentSeries = nullptr;
    QValueAxis *m_pfcBusAxis = nullptr;
    QValueAxis *m_llcVoltageAxis = nullptr;
    QValueAxis *m_llcCurrentAxis = nullptr;
    QVector<QPointF> m_pfcBusPoints;
    QVector<QPointF> m_llcVoltagePoints;
    QVector<QPointF> m_llcCurrentPoints;
};
