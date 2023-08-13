#pragma once

#include <bidib/address.h>
#include <bidib/error.h>
#include <bidib/pack.h>

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

    template<class... Types>
    static Message create(int type, Types const &...t)
    {
        return Message(type, Packer::pack(t...));
    }

    static QString name(quint8 type);

private:
    quint8 _type{};
    QByteArray _payload{};

    friend QDebug operator<<(QDebug d, Message const &msg);
};

QDebug operator<<(QDebug d, Message const &msg);

} // namespace Bd
