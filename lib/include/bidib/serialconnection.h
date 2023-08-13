#pragma once

#include <QtCore/QObject>

namespace Bd {

class SerialConnectionPrivate;

class SerialConnection : public QObject
{
    Q_OBJECT

signals:
    void dataReceived(QByteArray const &data);

public slots:
    void sendData(QByteArray const &data);

public:
    explicit SerialConnection(QString const &port);

private slots:
    void readData();

private:
    Q_DECLARE_PRIVATE(SerialConnection)
};

} // namespace Bd
