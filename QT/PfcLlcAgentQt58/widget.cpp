#include "widget.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPainter>
#include <QSerialPortInfo>
#include <QVBoxLayout>
#include <QtCharts/QChart>

namespace {

QString number(double value, int decimals = 1)
{
    return QString::number(value, 'f', decimals);
}

QString hex16(quint16 value)
{
    return QStringLiteral("0x%1").arg(value, 4, 16, QLatin1Char('0')).toUpper();
}

void appendPoint(QVector<QPointF> *points, double value)
{
    constexpr int maximumPoints = 120;
    points->append(QPointF(points->size(), value));
    if (points->size() > maximumPoints) {
        points->removeFirst();
    }
    for (int index = 0; index < points->size(); ++index) {
        (*points)[index].setX(index);
    }
}

void updateAxis(QValueAxis *axis, const QVector<QPointF> &points)
{
    double maximum = 1.0;
    for (const QPointF &point : points) {
        maximum = qMax(maximum, point.y());
    }
    axis->setRange(0.0, maximum * 1.15);
}

} // namespace

Widget::Widget(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
    m_config = ConfigManager::defaults();

    QString configDetail;
    const QStringList configCandidates{
        QDir::current().filePath(QStringLiteral("config/agent.json")),
        QDir::current().filePath(QStringLiteral("config/agent.example.json")),
        QCoreApplication::applicationDirPath() + QStringLiteral("/config/agent.json"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/config/agent.example.json")
    };
    for (const QString &path : configCandidates) {
        if (QFileInfo::exists(path)) {
            ConfigManager::load(path, &m_config, &configDetail);
            break;
        }
    }

    m_serial.setSettings(m_config.serial);
    m_localRules.setConfig(m_config.safety);
    m_mimo.setConfig(m_config.mimo);
    m_agentTimer.setInterval(m_config.mimo.requestIntervalMs);

    connect(m_refreshButton, &QPushButton::clicked, this, &Widget::refreshPorts);
    connect(m_openButton, &QPushButton::clicked, this, &Widget::toggleSerial);
    connect(m_analyzeButton, &QPushButton::clicked, this, &Widget::analyzeCurrentSnapshot);
    connect(&m_serial, &SerialModbusClient::telemetryReceived, this, &Widget::updateTelemetry);
    connect(&m_serial, &SerialModbusClient::connectionChanged, this, [this](bool online, const QString &detail) {
        m_connectionLabel->setText(online ? QStringLiteral("串口在线：") + detail
                                           : QStringLiteral("串口离线：") + detail);
        appendLog(m_connectionLabel->text());
    });
    connect(&m_serial, &SerialModbusClient::protocolError, this, &Widget::appendLog);
    connect(&m_serial, &SerialModbusClient::transportError, this, &Widget::appendLog);
    connect(&m_mimo, &MimoClient::serviceStateChanged, this, [this](const QString &state) {
        m_agentStateLabel->setText(state);
        appendLog(state);
    });
    connect(&m_mimo, &MimoClient::serviceError, this, [this](const QString &error) {
        m_agentStateLabel->setText(QStringLiteral("MiMo错误：") + error);
        appendLog(m_agentStateLabel->text());
    });
    connect(&m_mimo, &MimoClient::decisionReady, this, &Widget::updateAgentDecision);
    connect(&m_agentTimer, &QTimer::timeout, this, &Widget::analyzeCurrentSnapshot);

    refreshPorts();
    if (m_mimo.isEnabled()) {
        m_agentTimer.start();
        m_agentStateLabel->setText(QStringLiteral("MiMo已配置，等待有效PFC/LLC遥测"));
    } else {
        m_agentStateLabel->setText(QStringLiteral("MiMo未启用：设置MIMO_API_KEY后重启程序"));
    }
    if (!configDetail.isEmpty()) {
        appendLog(configDetail);
    }
    appendLog(QStringLiteral("PFC + LLC Qt 5.8上位机启动；控制模式固定为只建议"));
}

void Widget::buildUi()
{
    setWindowTitle(QStringLiteral("PFC + LLC 智能监控上位机 V1.58（只建议模式）"));
    auto *root = new QVBoxLayout(this);

    auto *serialRow = new QHBoxLayout;
    m_portBox = new QComboBox(this);
    m_refreshButton = new QPushButton(QStringLiteral("刷新串口"), this);
    m_openButton = new QPushButton(QStringLiteral("打开串口"), this);
    m_analyzeButton = new QPushButton(QStringLiteral("立即分析"), this);
    m_connectionLabel = new QLabel(QStringLiteral("串口未连接"), this);
    serialRow->addWidget(new QLabel(QStringLiteral("串口："), this));
    serialRow->addWidget(m_portBox);
    serialRow->addWidget(m_refreshButton);
    serialRow->addWidget(m_openButton);
    serialRow->addStretch();
    serialRow->addWidget(m_connectionLabel);
    root->addLayout(serialRow);

    auto *dataGrid = new QGridLayout;
    auto *pfcBox = new QGroupBox(QStringLiteral("PFC数据"), this);
    auto *pfcForm = new QFormLayout(pfcBox);
    m_pfcState = addReadOnlyValue(pfcForm, QStringLiteral("状态"));
    m_pfcFault = addReadOnlyValue(pfcForm, QStringLiteral("故障"));
    m_pfcAcInput = addReadOnlyValue(pfcForm, QStringLiteral("交流输入"));
    m_pfcBusVoltage = addReadOnlyValue(pfcForm, QStringLiteral("母线电压"));
    m_pfcInputCurrent = addReadOnlyValue(pfcForm, QStringLiteral("输入电流"));
    m_pfcTemperature = addReadOnlyValue(pfcForm, QStringLiteral("温度"));

    auto *llcBox = new QGroupBox(QStringLiteral("LLC数据"), this);
    auto *llcForm = new QFormLayout(llcBox);
    m_llcState = addReadOnlyValue(llcForm, QStringLiteral("状态"));
    m_llcFault = addReadOnlyValue(llcForm, QStringLiteral("故障"));
    m_llcInputVoltage = addReadOnlyValue(llcForm, QStringLiteral("输入电压"));
    m_llcInputCurrent = addReadOnlyValue(llcForm, QStringLiteral("输入电流"));
    m_llcOutputVoltage = addReadOnlyValue(llcForm, QStringLiteral("输出电压"));
    m_llcOutputCurrent = addReadOnlyValue(llcForm, QStringLiteral("输出电流"));
    m_llcTemperature = addReadOnlyValue(llcForm, QStringLiteral("温度"));
    m_llcOutputEnable = addReadOnlyValue(llcForm, QStringLiteral("输出使能"));
    m_llcPower = addReadOnlyValue(llcForm, QStringLiteral("输出功率"));

    auto *agentBox = new QGroupBox(QStringLiteral("MiMo智能分析（只建议，不下发控制）"), this);
    auto *agentForm = new QFormLayout(agentBox);
    m_localRuleLabel = addReadOnlyValue(agentForm, QStringLiteral("本地规则"));
    m_agentStateLabel = addReadOnlyValue(agentForm, QStringLiteral("服务状态"));
    m_agentDecisionLabel = addReadOnlyValue(agentForm, QStringLiteral("Agent建议"));
    m_finalDecisionLabel = addReadOnlyValue(agentForm, QStringLiteral("最终结果"));
    m_agentExplanationLabel = addReadOnlyValue(agentForm, QStringLiteral("原因"));
    agentForm->addRow(QString(), m_analyzeButton);

    dataGrid->addWidget(pfcBox, 0, 0);
    dataGrid->addWidget(llcBox, 0, 1);
    dataGrid->addWidget(agentBox, 0, 2);
    root->addLayout(dataGrid);

    auto *chartGrid = new QGridLayout;
    chartGrid->addWidget(createChart(QStringLiteral("PFC母线电压"), &m_pfcBusSeries, &m_pfcBusAxis), 0, 0);
    chartGrid->addWidget(createChart(QStringLiteral("LLC输出电压"), &m_llcVoltageSeries, &m_llcVoltageAxis), 0, 1);
    chartGrid->addWidget(createChart(QStringLiteral("LLC输出电流"), &m_llcCurrentSeries, &m_llcCurrentAxis), 0, 2);
    root->addLayout(chartGrid, 1);

    m_logView = new QPlainTextEdit(this);
    m_logView->setReadOnly(true);
    m_logView->setMaximumBlockCount(300);
    root->addWidget(m_logView, 1);
}

QLabel *Widget::addReadOnlyValue(QFormLayout *layout, const QString &caption)
{
    auto *value = new QLabel(QStringLiteral("—"), this);
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    value->setWordWrap(true);
    layout->addRow(caption + QStringLiteral("："), value);
    return value;
}

QChartView *Widget::createChart(const QString &title, QLineSeries **series, QValueAxis **axisY)
{
    auto *chart = new QChart;
    chart->setTitle(title);
    chart->legend()->hide();
    *series = new QLineSeries(chart);
    *axisY = new QValueAxis(chart);
    auto *axisX = new QValueAxis(chart);
    axisX->setRange(0, 120);
    axisX->setLabelFormat(QStringLiteral("%d"));
    (*axisY)->setRange(0, 1);
    (*axisY)->setLabelFormat(QStringLiteral("%.1f"));
    chart->addSeries(*series);
    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(*axisY, Qt::AlignLeft);
    (*series)->attachAxis(axisX);
    (*series)->attachAxis(*axisY);
    auto *view = new QChartView(chart, this);
    view->setRenderHint(QPainter::Antialiasing);
    view->setMinimumHeight(220);
    return view;
}

void Widget::refreshPorts()
{
    const QString selected = m_portBox->currentText();
    m_portBox->clear();
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        m_portBox->addItem(info.portName());
    }
    const int index = m_portBox->findText(selected);
    if (index >= 0) {
        m_portBox->setCurrentIndex(index);
    }
}

void Widget::toggleSerial()
{
    if (m_serial.isOpen()) {
        m_serial.close();
        m_openButton->setText(QStringLiteral("打开串口"));
        m_portBox->setEnabled(true);
        return;
    }
    if (m_portBox->currentText().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("串口"), QStringLiteral("没有可用串口"));
        return;
    }
    QString error;
    if (!m_serial.open(m_portBox->currentText(), &error)) {
        QMessageBox::critical(this, QStringLiteral("串口"), QStringLiteral("打开失败：%1").arg(error));
        return;
    }
    m_openButton->setText(QStringLiteral("关闭串口"));
    m_portBox->setEnabled(false);
    appendLog(QStringLiteral("已打开串口 %1").arg(m_serial.portName()));
}

void Widget::updateTelemetry(const TelemetrySnapshot &snapshot)
{
    m_snapshot = snapshot;
    m_pfcState->setText(QString::number(snapshot.pfcState));
    m_pfcFault->setText(hex16(snapshot.pfcFault));
    m_pfcAcInput->setText(number(snapshot.pfcAcInputVoltageV) + QStringLiteral(" V"));
    m_pfcBusVoltage->setText(number(snapshot.pfcBusVoltageV) + QStringLiteral(" V"));
    m_pfcInputCurrent->setText(number(snapshot.pfcInputCurrentA) + QStringLiteral(" A"));
    m_pfcTemperature->setText(number(snapshot.pfcTemperatureC) + QStringLiteral(" ℃"));

    m_llcState->setText(QString::number(snapshot.llcState));
    m_llcFault->setText(hex16(snapshot.llcFault));
    m_llcInputVoltage->setText(number(snapshot.llcInputVoltageV) + QStringLiteral(" V"));
    m_llcInputCurrent->setText(number(snapshot.llcInputCurrentA) + QStringLiteral(" A"));
    m_llcOutputVoltage->setText(number(snapshot.llcOutputVoltageV) + QStringLiteral(" V"));
    m_llcOutputCurrent->setText(number(snapshot.llcOutputCurrentA) + QStringLiteral(" A"));
    m_llcTemperature->setText(number(snapshot.llcTemperatureC) + QStringLiteral(" ℃"));
    m_llcOutputEnable->setText(snapshot.llcOutputEnabled ? QStringLiteral("使能") : QStringLiteral("禁止"));
    m_llcPower->setText(number(snapshot.llcOutputPowerW()) + QStringLiteral(" W"));

    m_localDecision = m_localRules.evaluate(snapshot);
    updateLocalDecision(m_localDecision);
    m_hasAgentDecision = false;
    updateFinalDecision();
    appendCharts(snapshot);
}

void Widget::updateLocalDecision(const RuleDecision &decision)
{
    m_localRuleLabel->setText(localActionToString(decision.action) + QStringLiteral("：") + decision.summary);
}

void Widget::updateAgentDecision(const AgentDecision &decision)
{
    if (!m_snapshot.valid || decision.snapshotSequence != m_snapshot.sequence) {
        appendLog(QStringLiteral("忽略过期Agent建议：快照序号不一致"));
        return;
    }
    m_agentDecision = decision;
    m_hasAgentDecision = true;
    m_agentDecisionLabel->setText(agentActionToString(decision.action)
                                  + QStringLiteral("，风险=") + decision.riskLevel
                                  + QStringLiteral("，置信度=") + number(decision.confidence, 2));
    m_agentExplanationLabel->setText(decision.explanation + QStringLiteral(" [")
                                     + decision.reasonCodes.join(QStringLiteral(", ")) + QStringLiteral("]"));
    updateFinalDecision();
}

void Widget::updateFinalDecision()
{
    const FinalDecision final = m_safetyGate.evaluate(m_localDecision,
                                                       m_hasAgentDecision ? &m_agentDecision : nullptr);
    m_finalDecisionLabel->setText(final.summary);
}

void Widget::analyzeCurrentSnapshot()
{
    if (!m_snapshot.valid) {
        appendLog(QStringLiteral("没有有效PFC/LLC遥测，未调用MiMo"));
        return;
    }
    if (m_localDecision.action == LocalAction::Stop) {
        appendLog(QStringLiteral("本地规则已要求停止，未调用MiMo"));
        return;
    }
    if (!m_mimo.analyze(m_snapshot, trendSummary())) {
        appendLog(QStringLiteral("MiMo未发起请求：服务未启用、请求进行中或遥测无效"));
    }
}

void Widget::appendLog(const QString &message)
{
    m_logView->appendPlainText(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"))
                               + QStringLiteral("  ") + message);
}

void Widget::appendCharts(const TelemetrySnapshot &snapshot)
{
    appendPoint(&m_pfcBusPoints, snapshot.pfcBusVoltageV);
    appendPoint(&m_llcVoltagePoints, snapshot.llcOutputVoltageV);
    appendPoint(&m_llcCurrentPoints, snapshot.llcOutputCurrentA);
    m_pfcBusSeries->replace(m_pfcBusPoints);
    m_llcVoltageSeries->replace(m_llcVoltagePoints);
    m_llcCurrentSeries->replace(m_llcCurrentPoints);
    updateAxis(m_pfcBusAxis, m_pfcBusPoints);
    updateAxis(m_llcVoltageAxis, m_llcVoltagePoints);
    updateAxis(m_llcCurrentAxis, m_llcCurrentPoints);
}

QJsonObject Widget::trendSummary() const
{
    const auto delta = [](const QVector<QPointF> &points) {
        if (points.size() < 2) {
            return 0.0;
        }
        return points.last().y() - points.first().y();
    };
    return {
        {"pfc_bus_voltage_change", delta(m_pfcBusPoints)},
        {"llc_output_voltage_change", delta(m_llcVoltagePoints)},
        {"llc_output_current_change", delta(m_llcCurrentPoints)},
        {"sample_count", m_llcCurrentPoints.size()}
    };
}
