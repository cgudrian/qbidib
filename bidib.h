#pragma once

#include <QByteArray>
#include <QDebug>
#include <QList>
#include <QString>

#include <experimental/propagate_const>
#include <vector>

#include "expected.hpp"

namespace Bd {

enum class Error {
    OutOfData,
    AddressTooLong,
    AddressStackEmpty,
    AddressStackFull,
    AddressMissingTerminator,
};

class Address
{
public:
    static Address localNode() { return {}; }

    qsizetype size() const
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

    bool isLocalNode() const { return _stack == 0; }

    tl::expected<quint8, Error> downstream()
    {
        if (isLocalNode())
            return tl::make_unexpected(Error::AddressStackEmpty);
        auto node = _stack & 0xff;
        _stack >>= 8;
        return node;
    }

    tl::expected<void, Error> upstream(quint8 node)
    {
        if (size() == 4)
            return tl::make_unexpected(Error::AddressStackFull);
        _stack = (_stack << 8) | node;
        return {};
    }

    bool operator==(const Address &rhs) const { return _stack == rhs._stack; }

    static tl::expected<Address, Error> parse(const QByteArray &ba)
    {
        if (ba.isEmpty())
            return tl::make_unexpected(Error::OutOfData);
        auto size = ba.indexOf('\0');
        if (size == -1)
            return tl::make_unexpected(Error::AddressMissingTerminator);
        if (size > 4)
            return tl::make_unexpected(Error::AddressTooLong);
        return Address(ba.first(size));
    }

private:
    Address() {}

    explicit Address(QByteArray bytes)
    {
        bytes.resize(4, 0);
        _stack = *reinterpret_cast<const quint32 *>(bytes.constData());
    }

    explicit Address(quint32 stack)
        : _stack(stack)
    {}

    quint32 _stack{};

    friend QDebug operator<<(QDebug d, const Address &a);
};

QDebug operator<<(QDebug d, const Address &a)
{
    d << QByteArray(reinterpret_cast<const char *>(&a), sizeof(a)).toHex('-');
    return d;
}

class Message
{
public:
    explicit Message(quint8 type, const QByteArray &payload)
        : _type(type)
        , _payload(payload)
    {}

    quint8 type() const { return _type; }
    const QByteArray &payload() const { return _payload; }
    QByteArray payload() { return _payload; }

private:
    quint8 _type{};
    QByteArray _payload{};
};

QString messageName(quint8 type);

} // namespace Bd
