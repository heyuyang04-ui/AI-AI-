#pragma once

#include "modbusstreamparser.h"
#include "telemetrysnapshot.h"

#include <QSerialPort>
#include <QTimer>

class SerialModbusClient : public QObject
{
    Q_OBJECT

public:
    struct Settings {
        int pollIntervalMs = 500;
        int responseTimeoutMs = 350;
        int maxRetries = 2;
    };

    explicit SerialModbusClient(QObject *parent = nullptr);

    void setSettings(const Settings &settings);
    bool open(const QString &portName, QString *error = nullptr);
    void close();
    bool isOpen() const;
    QString portName() const;

    // This interface is prepared for the future STM32 10H command register block.
    // The Qt UI does not call it while the application is in advisory-only mode.
    bool sendControlCommand(quint16 sequence, PfcLlcProtocol::CommandAction action,
                            double targetVoltageV, double targetCurrentA,
                            quint16 armToken, QString *error = nullptr);

signals:
    void telemetryReceived(const TelemetrySnapshot &snapshot);
    void connectionChanged(bool online, const QString &detail);
    void commandAcknowledged(quint16 firstRegister, quint16 count);
    void protocolError(const QString &message);
    void transportError(const QString &message);

private slots:
    void requestTelemetry();
    void onReadyRead();
    void onResponseTimeout();
    void onSerialError(QSerialPort::SerialPortError error);

private:
    enum class PendingKind { None, Telemetry, Control };

    bool writeFrame(const QByteArray &frame, PendingKind kind, QString *error = nullptr);
    void processFrame(const PfcLlcProtocol::Frame &frame);
    void markHealthy();
    void markFailure(const QString &detail);

    QSerialPort m_serial;
    QTimer m_pollTimer;
    QTimer m_responseTimer;
    Settings m_settings;
    ModbusStreamParser m_parser;
    PendingKind m_pendingKind = PendingKind::None;
    QByteArray m_pendingFrame;
    int m_retryCount = 0;
    int m_consecutiveFailures = 0;
    bool m_online = false;
};
