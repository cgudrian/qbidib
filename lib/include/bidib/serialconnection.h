#pragma once

#include <QObject>
#include <QSerialPort>

namespace Bd {

class SerialConnection : public QObject
{
    Q_OBJECT

signals:
    void dataReceived(const QByteArray &data);

public slots:
    void sendData(const QByteArray &data);

public:
    explicit SerialConnection(const QString &port);

private slots:
    void readData();

private:
    QSerialPort _serial;
};

} // namespace Bd
