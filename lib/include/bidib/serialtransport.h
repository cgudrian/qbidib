#pragma once

#include <bidib/error.h>

#include <QtCore/QObject>

#include <expected.hpp>

namespace Bd {

class SerialTransportPrivate;

class SerialTransport : public QObject
{
    Q_OBJECT

signals:
    void frameReceived(const QByteArray &frame);

public slots:
    void processData(const QByteArray &data);

public:
    static QByteArray escape(const QByteArray &ba);
    static tl::expected<QByteArray, Error> unescape(const QByteArray &ba);

    SerialTransport();

private:
    Q_DECLARE_PRIVATE(SerialTransport)
};

} // namespace Bd
