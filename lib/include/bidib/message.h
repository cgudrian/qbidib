#pragma once

#include <bidib/address.h>
#include <bidib/error.h>

#include <QtCore/QByteArray>

#include <expected.hpp>

class QString;

namespace Bd {

class Message
{
public:
    explicit Message(quint8 type, QByteArray const &payload);
    quint8 type() const;
    const QByteArray &payload() const;
    tl::expected<QByteArray, Error> toSendBuffer(Address address, quint8 number) const;

private:
    quint8 _type{};
    QByteArray _payload{};
};

QString messageName(quint8 type);

} // namespace Bd
