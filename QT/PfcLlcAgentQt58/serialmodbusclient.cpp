#include "serialmodbusclient.h"

#include "modbusprotocol.h"

#include <QtMath>

SerialModbusClient::SerialModbusClient(QObject *parent)
    : QObject(parent)
{
    m_pollTimer.setParent(this);
    m_responseTimer.setParent(this);
    m_pollTimer.setTimerType(Qt::PreciseTimer);
    m_responseTimer.setSingleShot(true);

    connect(&m_pollTimer, &QTimer::timeout, this, &SerialModbusClient::requestTelemetry);
    connect(&m_responseTimer, &QTimer::timeout, this, &SerialModbusClient::onResponseTimeout);
    connect(&m_serial, &QSerialPort::readyRead, this, &SerialModbusClient::onReadyRead);
    connect(&m_serial, &QSerialPort::errorOccurred, this, &SerialModbusClient::onSerialError);
}

void SerialModbusClient::setSettings(const Settings &settings)
{
    m_settings.pollIntervalMs = qBound(100, settings.pollIntervalMs, 5000);
    m_settings.responseTimeoutMs = qBound(50, settings.responseTimeoutMs, 5000);
    m_settings.maxRetries = qBound(0, settings.maxRetries, 5);
    if (m_pollTimer.isActive()) {
        m_pollTimer.start(m_settings.pollIntervalMs);
    }
}

bool SerialModbusClient::open(const QString &portName, QString *error)
{
    close();
    m_serial.setPortName(portName);
    m_serial.setBaudRate(QSerialPort::Baud115200);
    m_serial.setDataBits(QSerialPort::Data8);
    m_serial.setParity(QSerialPort::NoParity);
    m_serial.setStopBits(QSerialPort::OneStop);
    m_serial.setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serial.open(QIODevice::ReadWrite)) {
        if (error != nullptr) {
            *error = m_serial.errorString();
        }
        return false;
    }

    m_parser.clear();
    m_consecutiveFailures = 0;
    m_retryCount = 0;
    m_pendingKind = PendingKind::None;
    m_pollTimer.start(m_settings.pollIntervalMs);
    requestTelemetry();
    return true;
}

void SerialModbusClient::close()
{
    m_pollTimer.stop();
    m_responseTimer.stop();
    m_pendingKind = PendingKind::None;
    m_pendingFrame.clear();
    m_parser.clear();
    if (m_serial.isOpen()) {
        m_serial.clear();
        m_serial.close();
    }
    if (m_online) {
        m_online = false;
        emit connectionChanged(false, QStringLiteral("串口已关闭"));
    }
}

bool SerialModbusClient::isOpen() const
{
    return m_serial.isOpen();
}

QString SerialModbusClient::portName() const
{
    return m_serial.portName();
}

bool SerialModbusClient::sendControlCommand(quint16 sequence,
                                             PfcLlcProtocol::CommandAction action,
                                             double targetVoltageV,
                                             double targetCurrentA,
                                             quint16 armToken,
                                             QString *error)
{
    if (m_pendingKind != PendingKind::None) {
        if (error != nullptr) {
            *error = QStringLiteral("当前存在未完成的Modbus事务");
        }
        return false;
    }
    if (!qIsFinite(targetVoltageV) || !qIsFinite(targetCurrentA)
        || targetVoltageV < 0.0 || targetVoltageV > 6553.5
        || targetCurrentA < 0.0 || targetCurrentA > 6553.5) {
        if (error != nullptr) {
            *error = QStringLiteral("控制目标超出协议表示范围");
        }
        return false;
    }

    const QVector<quint16> values{
        sequence,
        static_cast<quint16>(action),
        static_cast<quint16>(qRound(targetVoltageV * 10.0)),
        static_cast<quint16>(qRound(targetCurrentA * 10.0)),
        armToken,
        1
    };
    return writeFrame(PfcLlcProtocol::buildWriteMultiple(PfcLlcProtocol::kControlBaseAddress, values),
                      PendingKind::Control, error);
}

void SerialModbusClient::requestTelemetry()
{
    if (!m_serial.isOpen() || m_pendingKind != PendingKind::None) {
        return;
    }
    writeFrame(PfcLlcProtocol::buildReadHolding(PfcLlcProtocol::kTelemetryBaseAddress,
                                                PfcLlcProtocol::kTelemetryRegisterCount),
               PendingKind::Telemetry);
}

void SerialModbusClient::onReadyRead()
{
    const QVector<PfcLlcProtocol::Frame> frames = m_parser.feed(m_serial.readAll());
    for (const PfcLlcProtocol::Frame &frame : frames) {
        processFrame(frame);
    }
}

void SerialModbusClient::onResponseTimeout()
{
    if (m_pendingKind == PendingKind::None) {
        return;
    }
    if (m_retryCount < m_settings.maxRetries && !m_pendingFrame.isEmpty()) {
        ++m_retryCount;
        m_serial.write(m_pendingFrame);
        m_responseTimer.start(m_settings.responseTimeoutMs);
        return;
    }

    m_pendingKind = PendingKind::None;
    m_pendingFrame.clear();
    markFailure(QStringLiteral("Modbus响应超时"));
}

void SerialModbusClient::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError || error == QSerialPort::ResourceError) {
        if (error == QSerialPort::ResourceError) {
            markFailure(m_serial.errorString());
        }
        return;
    }
    emit transportError(m_serial.errorString());
}

bool SerialModbusClient::writeFrame(const QByteArray &frame, PendingKind kind, QString *error)
{
    if (!m_serial.isOpen()) {
        if (error != nullptr) {
            *error = QStringLiteral("串口未打开");
        }
        return false;
    }
    if (frame.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("无效Modbus请求帧");
        }
        return false;
    }
    if (m_serial.write(frame) != frame.size()) {
        if (error != nullptr) {
            *error = m_serial.errorString();
        }
        markFailure(QStringLiteral("串口发送失败：%1").arg(m_serial.errorString()));
        return false;
    }

    m_pendingKind = kind;
    m_pendingFrame = frame;
    m_retryCount = 0;
    m_responseTimer.start(m_settings.responseTimeoutMs);
    return true;
}

void SerialModbusClient::processFrame(const PfcLlcProtocol::Frame &frame)
{
    if (frame.address != PfcLlcProtocol::kSlaveAddress) {
        return;
    }
    if ((frame.function & 0x80) != 0) {
        const quint8 exceptionCode = frame.payload.isEmpty()
                                       ? 0
                                       : static_cast<quint8>(frame.payload.at(0));
        m_responseTimer.stop();
        m_pendingKind = PendingKind::None;
        m_pendingFrame.clear();
        markFailure(QStringLiteral("STM32返回Modbus异常码0x%1")
                        .arg(exceptionCode, 2, 16, QLatin1Char('0')));
        return;
    }

    if (frame.function == PfcLlcProtocol::kReadHoldingRegisters) {
        QVector<quint16> registers;
        QString error;
        if (!PfcLlcProtocol::decodeReadHolding(frame, &registers, &error)) {
            emit protocolError(error);
            return;
        }
        TelemetrySnapshot snapshot;
        if (!TelemetrySnapshot::fromRegisters(registers, &snapshot, &error)) {
            emit protocolError(error);
            return;
        }
        m_responseTimer.stop();
        m_pendingKind = PendingKind::None;
        m_pendingFrame.clear();
        markHealthy();
        emit telemetryReceived(snapshot);
        return;
    }

    if (frame.function == PfcLlcProtocol::kWriteMultipleRegisters) {
        quint16 address = 0;
        quint16 count = 0;
        QString error;
        if (!PfcLlcProtocol::decodeWriteMultipleAck(frame, &address, &count, &error)) {
            emit protocolError(error);
            return;
        }
        m_responseTimer.stop();
        m_pendingKind = PendingKind::None;
        m_pendingFrame.clear();
        markHealthy();
        emit commandAcknowledged(address, count);
    }
}

void SerialModbusClient::markHealthy()
{
    m_consecutiveFailures = 0;
    if (!m_online) {
        m_online = true;
        emit connectionChanged(true, QStringLiteral("STM32遥测在线"));
    }
}

void SerialModbusClient::markFailure(const QString &detail)
{
    ++m_consecutiveFailures;
    emit transportError(detail);
    if (m_consecutiveFailures >= 3 && m_online) {
        m_online = false;
        emit connectionChanged(false, detail);
    }
}
