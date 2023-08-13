#pragma once

#include <bidib/error.h>

#include <QByteArray>

#include <expected.hpp>

namespace Bd {

class Address
{
public:
    static Address localNode();
    QByteArray toByteArray() const;
    qsizetype size() const;
    bool isLocalNode() const;
    tl::expected<quint8, Error> downstream();
    tl::expected<void, Error> upstream(quint8 node);
    bool operator==(const Address &rhs) const;
    static tl::expected<Address, Error> parse(const QByteArray &ba);

private:
    Address();
    explicit Address(QByteArray bytes);
    explicit Address(quint32 stack);

    quint32 _stack{};

    friend QDebug operator<<(QDebug d, const Address &a);
};

QDebug operator<<(QDebug d, const Address &a);

} // namespace Bd
