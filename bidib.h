#pragma once

#include <QByteArray>
#include <QDebug>
#include <QList>
#include <QObject>
#include <QSerialPort>
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
    MessageTooLarge,
};

class Address
{
public:
    static Address localNode() { return {}; }

    QByteArray toByteArray() const
    {
        auto res = QByteArray(reinterpret_cast<const char *>(&_stack), sizeof(_stack));
        res.resize(size());
        res.append('\0');
        return res;
    }

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

    friend  QDebug operator<<(QDebug d, const Address &a);
};

QDebug operator<<(QDebug d, const Address &a);

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

    tl::expected<QByteArray, Error> toSendBuffer(Address address, quint8 number) const;

private:
    quint8 _type{};
    QByteArray _payload{};
};

class SerialConnection : public QObject
{
    Q_OBJECT

signals:
    void dataReceived(const QByteArray &data);

public slots:
    void sendData(const QByteArray &data);

public:
    explicit SerialConnection(const QString &port);

private slots:
    void readData();

private:
    QSerialPort _serial;
};

class SerialTransport : public QObject
{
    Q_OBJECT

signals:
    void frameReceived(const QByteArray &frame);

public slots:
    void processData(const QByteArray &data);

private:
    QByteArray _currentFrame;
};

QString messageName(quint8 type);

} // namespace Bd
