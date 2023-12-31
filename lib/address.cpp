#include "address.h"

#include <QDebug>

namespace Bd {

Address::Address() {}

Address::Address(QByteArrayView bytes)
{
    Q_ASSERT(bytes.size() <= 4);

    _stack = 0;
    switch (bytes.size()) {
    case 4:
        _stack |= bytes[3] << 24;
        // fall through
    case 3:
        _stack |= bytes[2] << 16;
        // fall through
    case 2:
        _stack |= bytes[1] << 8;
        // fall through
    case 1:
        _stack |= bytes[0];
        break;
    }
}

Address::Address(quint32 stack)
    : _stack(stack)
{}

Address Address::localNode()
{
    return {};
}

QByteArray Address::toByteArray() const
{
    auto res = QByteArray(reinterpret_cast<const char *>(&_stack), sizeof(_stack));
    res.resize(size());
    res.append('\0');
    return res;
}

qsizetype Address::size() const
{
    if (_stack & 0xff000000)
        return 4;
    if (_stack & 0xff0000)
        return 3;
    if (_stack & 0xff00)
        return 2;
    if (_stack & 0xff)
        return 1;
    return 0;
}

bool Address::isLocalNode() const
{
    return _stack == 0;
}

tl::expected<quint8, Error> Address::downstream()
{
    if (isLocalNode())
        return tl::make_unexpected(Error::AddressStackEmpty);
    auto node = _stack & 0xff;
    _stack >>= 8;
    return node;
}

tl::expected<void, Error> Address::upstream(quint8 node)
{
    if (size() == 4)
        return tl::make_unexpected(Error::AddressStackFull);
    _stack = (_stack << 8) | node;
    return {};
}

bool Address::operator==(Address const &rhs) const
{
    return _stack == rhs._stack;
}

tl::expected<Address, Error> Address::parse(QByteArrayView bytes)
{
    if (bytes.isEmpty())
        return tl::make_unexpected(Error::OutOfData);
    auto size = bytes.indexOf('\0');
    if (size == -1)
        return tl::make_unexpected(Error::AddressMissingTerminator);
    if (size > 4)
        return tl::make_unexpected(Error::AddressTooLong);
    return Address(bytes.first(size));
}

QDebug operator<<(QDebug d, Address const &a)
{
    d << QByteArray(reinterpret_cast<const char *>(&a), sizeof(a)).toHex('-');
    return d;
}

} // namespace Bd
