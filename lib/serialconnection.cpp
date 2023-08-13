#include "serialconnection.h"

#include <QSerialPort>

#include <QtCore/private/qobject_p.h>

namespace Bd {

class SerialConnectionPrivate : public QObjectPrivate
{
public:
    Q_DECLARE_PUBLIC(SerialConnection)
    QSerialPort serial;
};

SerialConnection::SerialConnection(const QString &port)
    : QObject(*new SerialConnectionPrivate)
{
    Q_D(SerialConnection);
    connect(&d->serial, &QSerialPort::readyRead, this, &SerialConnection::readData);
}

void SerialConnection::readData()
{
    Q_D(SerialConnection);
    auto data = d->serial.readAll();
    if (!data.isEmpty())
        emit dataReceived(data);
}

void SerialConnection::sendData(const QByteArray &data)
{
    Q_D(SerialConnection);
    d->serial.write(data);
}

} // namespace Bd
