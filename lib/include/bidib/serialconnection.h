#pragma once

#include <QtCore/QObject>

namespace Bd {

class SerialConnectionPrivate;

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
    Q_DECLARE_PRIVATE(SerialConnection)
};

} // namespace Bd
