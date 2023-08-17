#pragma once

#include <bidib/error.h>

#include <QtCore/QObject>

#include <expected.hpp>

namespace Bd {

class SerialTransportPrivate;
class Message;
class Address;

class SerialTransport : public QObject
{
    Q_OBJECT

signals:
    void frameReceived(QByteArray frame);
    void messageReceived(Address address, Message msg);
    void errorOccurred(Error, QByteArray frame);

public slots:
    void processData(QByteArray data);
    void processFrame(QByteArray frame);

public:
    static QByteArray escape(QByteArray const &ba);
    static tl::expected<QByteArray, Error> unescape(QByteArray const &ba);

    SerialTransport(QObject *parent = nullptr);

private:
    Q_DECLARE_PRIVATE(SerialTransport)
};

} // namespace Bd
