#include "modbusprotocol.h"

namespace {

void appendU16(QByteArray *bytes, quint16 value)
{
    bytes->append(static_cast<char>((value >> 8) & 0xff));
    bytes->append(static_cast<char>(value & 0xff));
}

quint16 readU16(const QByteArray &bytes, int offset)
{
    return (static_cast<quint16>(static_cast<quint8>(bytes.at(offset))) << 8)
           | static_cast<quint8>(bytes.at(offset + 1));
}

QByteArray appendCrc(QByteArray bytes)
{
    const quint16 crc = PfcLlcProtocol::crc16(bytes);
    bytes.append(static_cast<char>(crc & 0xff));
    bytes.append(static_cast<char>((crc >> 8) & 0xff));
    return bytes;
}

} // namespace

namespace PfcLlcProtocol {

quint16 crc16(const QByteArray &bytes)
{
    quint16 crc = 0xffff;
    for (const char byte : bytes) {
        crc ^= static_cast<quint8>(byte);
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x0001) ? static_cast<quint16>((crc >> 1) ^ 0xa001)
                                 : static_cast<quint16>(crc >> 1);
        }
    }
    return crc;
}

QByteArray buildReadHolding(quint16 firstRegister, quint16 registerCount)
{
    QByteArray frame;
    frame.reserve(8);
    frame.append(static_cast<char>(kSlaveAddress));
    frame.append(static_cast<char>(kReadHoldingRegisters));
    appendU16(&frame, firstRegister);
    appendU16(&frame, registerCount);
    return appendCrc(frame);
}

QByteArray buildWriteMultiple(quint16 firstRegister, const QVector<quint16> &values)
{
    if (values.isEmpty() || values.size() > 123) {
        return {};
    }

    QByteArray frame;
    frame.reserve(9 + values.size() * 2);
    frame.append(static_cast<char>(kSlaveAddress));
    frame.append(static_cast<char>(kWriteMultipleRegisters));
    appendU16(&frame, firstRegister);
    appendU16(&frame, static_cast<quint16>(values.size()));
    frame.append(static_cast<char>(values.size() * 2));
    for (const quint16 value : values) {
        appendU16(&frame, value);
    }
    return appendCrc(frame);
}

bool verifyFrame(const QByteArray &raw)
{
    if (raw.size() < 5) {
        return false;
    }
    const int dataSize = raw.size() - 2;
    const quint16 received = static_cast<quint8>(raw.at(dataSize))
                             | (static_cast<quint16>(static_cast<quint8>(raw.at(dataSize + 1))) << 8);
    return crc16(raw.left(dataSize)) == received;
}

bool decodeReadHolding(const Frame &frame, QVector<quint16> *registers, QString *error)
{
    if (registers == nullptr || frame.function != kReadHoldingRegisters || frame.payload.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("不是有效的03H响应");
        }
        return false;
    }

    const int byteCount = static_cast<quint8>(frame.payload.at(0));
    if (byteCount + 1 != frame.payload.size() || (byteCount % 2) != 0) {
        if (error != nullptr) {
            *error = QStringLiteral("03H响应字节数不匹配");
        }
        return false;
    }

    registers->clear();
    registers->reserve(byteCount / 2);
    for (int offset = 1; offset < frame.payload.size(); offset += 2) {
        registers->append(readU16(frame.payload, offset));
    }
    return true;
}

bool decodeWriteMultipleAck(const Frame &frame, quint16 *firstRegister, quint16 *count, QString *error)
{
    if (frame.function != kWriteMultipleRegisters || frame.payload.size() != 4) {
        if (error != nullptr) {
            *error = QStringLiteral("不是有效的10H确认帧");
        }
        return false;
    }
    if (firstRegister != nullptr) {
        *firstRegister = readU16(frame.payload, 0);
    }
    if (count != nullptr) {
        *count = readU16(frame.payload, 2);
    }
    return true;
}

} // namespace PfcLlcProtocol
