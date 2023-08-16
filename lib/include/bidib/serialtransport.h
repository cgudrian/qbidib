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
    void processData(QByteArray const &data);

public:
    static QByteArray escape(QByteArray const &ba);
    static tl::expected<QByteArray, Error> unescape(QByteArray const &ba);

    SerialTransport();

private:
    Q_DECLARE_PRIVATE(SerialTransport)
};

} // namespace Bd
