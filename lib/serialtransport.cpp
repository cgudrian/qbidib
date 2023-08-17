#include "serialtransport.h"
#include "bidib_messages.h"
#include "crc.h"
#include "message.h"

#include <QtCore/private/qobject_p.h>

namespace Bd {

class SerialTransportPrivate : public QObjectPrivate
{
public:
    Q_DECLARE_PUBLIC(SerialTransport)

    void processData(QByteArray data);
    void processFrame(QByteArray frame);
    tl::expected<std::tuple<Address, Message>, Error> parseMessageData(QByteArrayView data);

    QByteArray currentFrame;
};

void SerialTransportPrivate::processData(QByteArray data)
{
    Q_Q(SerialTransport);

    auto from = 0;
    auto to = data.indexOf(static_cast<quint8>(BIDIB_PKT_MAGIC), from);

    // skip leading garbage
    if (currentFrame.isEmpty() && to > 0)
        from = to;

    while (to >= 0) {
        auto count = to - from;
        if (count) {
            currentFrame.append(data.sliced(from, count));
            if (!currentFrame.isEmpty()) {
                emit q->frameReceived(currentFrame);
                processFrame(currentFrame);
                currentFrame.clear();
            }
        }
        from = to + 1;
        to = data.indexOf(static_cast<quint8>(BIDIB_PKT_MAGIC), from);
    }
    currentFrame.append(data.sliced(from));
}

void SerialTransportPrivate::processFrame(QByteArray frame)
{
    Q_Q(SerialTransport);

    if (frame.isEmpty())
        // empty frame is no error
        return;

    if (computeCrc8(frame) != 0) {
        emit q->errorOccurred(Error::BadChecksum, frame);
        return;
    }

    // remove checksum byte
    auto data = frame.first(frame.size() - 1);

    int pos = 0;
    // the last byte contains the checksum
    while (pos < data.size()) {
        quint8 len = data[pos];

        auto msgData = data.mid(pos + 1, len);
        if (msgData.size() < len) {
            emit q->errorOccurred(Error::OutOfData, msgData);
            return;
        }

        auto parsed = parseMessageData(msgData);
        if (parsed) {
            auto [address, message] = *parsed;
            emit q->messageReceived(address, message);
        } else {
            emit q->errorOccurred(parsed.error(), msgData);
        }

        pos += len + 1;
    }
}

tl::expected<std::tuple<Address, Message>, Error> SerialTransportPrivate::parseMessageData(
    QByteArrayView data)
{
    auto address = Address::parse(data);
    if (!address)
        return tl::make_unexpected(address.error());

    auto i = address->size() + 1; // skip trailing zero
    if (data.size() - i < 2)
        return tl::make_unexpected(Error::MessageMalformed);

    // TODO: implement sequence check
    auto num = data[i++];
    auto type = data[i++];
    QByteArray payload;
    if (i < data.size())
        payload = data.sliced(i).toByteArray();
    return std::make_tuple(*address, Message(type, payload));
}

SerialTransport::SerialTransport(QObject *parent)
    : QObject(*new SerialTransportPrivate, parent)
{}

void SerialTransport::processData(QByteArray data)
{
    Q_D(SerialTransport);
    d->processData(data);
}

void SerialTransport::processFrame(QByteArray frame)
{
    Q_D(SerialTransport);
    d->processFrame(frame);
}

QByteArray SerialTransport::escape(QByteArray const &ba)
{
    if (ba.isEmpty())
        return {};

    QByteArray result{ba};
    int i = 0;
    int o = 0;
    while (i < ba.size()) {
        quint8 c = ba[i++];
        if (c == BIDIB_PKT_MAGIC || c == BIDIB_PKT_ESCAPE) {
            result.insert(o++, static_cast<quint8>(BIDIB_PKT_ESCAPE));
            result[o] = c ^ 0x20;
        }
        o++;
    }
    return result;
}

tl::expected<QByteArray, Error> SerialTransport::unescape(QByteArray const &ba)
{
    if (ba.isEmpty())
        return {};

    if (static_cast<quint8>(ba.back()) == BIDIB_PKT_ESCAPE)
        return tl::make_unexpected(Error::EscapingIncomplete);

    QByteArray result{ba};
    int i = 0;
    int o = 0;
    while (i < ba.size()) {
        quint8 c = ba[i++];
        if (c == BIDIB_PKT_ESCAPE) {
            result.remove(o, 1);
            result[o] = ba[i++] ^ 0x20;
        }
        o++;
    }
    return result;
}

} // namespace Bd
