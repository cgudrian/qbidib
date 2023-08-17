#pragma once

#include <bidib/error.h>

#include <QtCore/QtTypes>

#include <expected.hpp>

class QByteArray;
class QDebug;

namespace Bd {

class Address
{
public:
    explicit Address(quint32 stack);

    static Address localNode();
    QByteArray toByteArray() const;
    qsizetype size() const;
    bool isLocalNode() const;
    tl::expected<quint8, Error> downstream();
    tl::expected<void, Error> upstream(quint8 node);
    bool operator==(Address const &rhs) const;
    static tl::expected<Address, Error> parse(QByteArrayView bytes);

private:
    Address();
    explicit Address(QByteArrayView bytes);

    quint32 _stack{};

    friend QDebug operator<<(QDebug d, Address const &a);
};

QDebug operator<<(QDebug d, Address const &a);

} // namespace Bd
