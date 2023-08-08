#pragma once

#include <QByteArray>
#include <QDebug>
#include <QList>
#include <QString>

#include <experimental/propagate_const>
#include <vector>

#include "expected.hpp"

namespace bdb {

enum class error {
    out_of_data,
    address_too_large,
    address_stack_empty,
    address_stack_full,
};

class address
{
public:
    address() {}

    address(quint8 a1, quint8 a2)
        : _stack(make_stack(a1, a2))
    {}

    address(quint8 a1, quint8 a2, quint8 a3)
        : _stack(make_stack(a1, a2, a3))
    {}

    address(quint8 a1, quint8 a2, quint8 a3, quint8 a4)
        : _stack(make_stack(a1, a2, a3, a4))
    {}

    qsizetype size() const
    {
        if (_stack == 0)
            return 0;
        if (_stack <= 0xff)
            return 1;
        if (_stack <= 0xffff)
            return 2;
        if (_stack <= 0xffffff)
            return 3;
        return 4;
    }
    bool isMe() const { return _stack == 0; }

    tl::expected<address, error> downstream() const
    {
        if (isMe())
            return tl::make_unexpected(error::address_stack_empty);
        return address(_stack >> 8);
    }

    tl::expected<address, error> upstream(quint8 next) const
    {
        if (size() == 4)
            return tl::make_unexpected(error::address_stack_full);
        return address(_stack << 8 | next);
    }

    bool operator==(const address &rhs) const { return _stack == rhs._stack; }

    static tl::expected<address, error> parse(const QByteArray &ba)
    {
        if (ba.isEmpty())
            return tl::make_unexpected(error::out_of_data);
        auto size = ba.indexOf('\0');
        if (size > 4)
            return tl::make_unexpected(error::address_too_large);
        return address(ba.left(size));
    }

    static address create(quint8 a1) { return address(a1); }

private:
    explicit address(QByteArray bytes)
    {
        bytes.resize(4);
        _stack = *reinterpret_cast<const quint32 *>(bytes.constData());
    }

    explicit address(quint32 stack)
        : _stack(stack)
    {}

    static quint32 make_stack(quint8 a1) { return a1; }

    static quint32 make_stack(quint8 a1, quint8 a2) { return make_stack(a1) << 8 | a2; }

    static quint32 make_stack(quint8 a1, quint8 a2, quint8 a3)
    {
        return make_stack(a1, a2) << 8 | a3;
    }

    static quint32 make_stack(quint8 a1, quint8 a2, quint8 a3, quint8 a4)
    {
        return make_stack(a1, a2, a3) << 8 | a4;
    }

    quint32 _stack{};

    friend QDebug operator<<(QDebug d, const address &a);
};

QDebug operator<<(QDebug d, const address &a)
{
    d << QString::number(a._stack, 16);
    return d;
}

class message
{
public:
    explicit message(quint8 type, const QByteArray &payload)
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

} // namespace bdb
