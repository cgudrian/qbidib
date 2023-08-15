#include "message.h"
#include "messagenames.h"

#include <QString>
#include <QDebug>

namespace Bd {

static constexpr auto MAX_MESSAGE_SIZE = 63;

Message::Message(quint8 type, QByteArray const &payload)
    : _type(type)
    , _payload(payload)
{}

QString Message::name(quint8 type)
{
    return Bd::messageName(type) ? Bd::messageName(type) : QString::number(type);
}

tl::expected<QByteArray, Error> Message::toSendBuffer(Address address, quint8 number) const
{
    auto size = 3 + address.size() + _payload.size();
    if (size > MAX_MESSAGE_SIZE)
        return tl::make_unexpected(Error::MessageTooLarge);
    QByteArray buf;
    buf.append(size);
    buf.append(address.toByteArray());
    buf.append(number);
    buf.append(_type);
    buf.append(_payload);
    return buf;
}

quint8 Message::type() const
{
    return _type;
}

const QByteArray &Message::payload() const
{
    return _payload;
}

QDebug operator<<(QDebug d, Message const &msg)
{
    return d;
}

} // namespace Bd
