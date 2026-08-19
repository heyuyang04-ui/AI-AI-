#pragma once

#include "modbusprotocol.h"

#include <QVector>

class ModbusStreamParser
{
public:
    QVector<PfcLlcProtocol::Frame> feed(const QByteArray &data);
    void clear();

private:
    int expectedFrameLength() const;

    QByteArray m_buffer;
};
