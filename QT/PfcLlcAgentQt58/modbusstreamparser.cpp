#include "modbusstreamparser.h"

QVector<PfcLlcProtocol::Frame> ModbusStreamParser::feed(const QByteArray &data)
{
    QVector<PfcLlcProtocol::Frame> frames;
    m_buffer.append(data);

    constexpr int maxBufferedBytes = 512;
    while (!m_buffer.isEmpty()) {
        const int start = m_buffer.indexOf(static_cast<char>(PfcLlcProtocol::kSlaveAddress));
        if (start < 0) {
            m_buffer.clear();
            break;
        }
        if (start > 0) {
            m_buffer.remove(0, start);
        }
        if (m_buffer.size() < 3) {
            break;
        }

        const int length = expectedFrameLength();
        if (length < 0) {
            m_buffer.remove(0, 1);
            continue;
        }
        if (m_buffer.size() < length) {
            break;
        }

        const QByteArray raw = m_buffer.left(length);
        if (!PfcLlcProtocol::verifyFrame(raw)) {
            m_buffer.remove(0, 1);
            continue;
        }

        PfcLlcProtocol::Frame frame;
        frame.address = static_cast<quint8>(raw.at(0));
        frame.function = static_cast<quint8>(raw.at(1));
        frame.payload = raw.mid(2, raw.size() - 4);
        frame.raw = raw;
        frames.append(frame);
        m_buffer.remove(0, length);
    }

    if (m_buffer.size() > maxBufferedBytes) {
        m_buffer.clear();
    }
    return frames;
}

void ModbusStreamParser::clear()
{
    m_buffer.clear();
}

int ModbusStreamParser::expectedFrameLength() const
{
    if (m_buffer.size() < 2) {
        return 0;
    }
    const quint8 function = static_cast<quint8>(m_buffer.at(1));
    if (function == PfcLlcProtocol::kReadHoldingRegisters) {
        if (m_buffer.size() < 3) {
            return 0;
        }
        const int byteCount = static_cast<quint8>(m_buffer.at(2));
        if (byteCount == 0 || byteCount > 250) {
            return -1;
        }
        return 5 + byteCount;
    }
    if (function == PfcLlcProtocol::kWriteMultipleRegisters || function == 0x06) {
        return 8;
    }
    if ((function & 0x80) != 0) {
        return 5;
    }
    return -1;
}
