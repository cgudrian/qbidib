#include <bidib/serialconnection.h>

namespace Bd {

SerialConnection::SerialConnection(const QString &port)
    : _serial(port)
{
    connect(&_serial, &QSerialPort::readyRead, this, &SerialConnection::readData);
}

void SerialConnection::readData()
{
    auto data = _serial.readAll();
    if (!data.isEmpty())
        emit dataReceived(data);
}

void SerialConnection::sendData(const QByteArray &data)
{
    _serial.write(data);
}

} // namespace Bd
