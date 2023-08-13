#include "serialtransport.h"
#include "bidib_messages.h"

#include <QtCore/private/qobject_p.h>

namespace Bd {

class SerialTransportPrivate : public QObjectPrivate
{
public:
    Q_DECLARE_PUBLIC(SerialTransport)

    QByteArray currentFrame;
};

SerialTransport::SerialTransport()
    : QObject(*new SerialTransportPrivate)
{}

void SerialTransport::processData(const QByteArray &data)
{
    Q_D(SerialTransport);

    auto from = 0;
    auto to = data.indexOf(BIDIB_PKT_MAGIC, from);

    // skip leading garbage
    if (d->currentFrame.isEmpty() && to > 0)
        from = to;

    while (to >= 0) {
        auto count = to - from;
        if (count) {
            d->currentFrame.append(data.sliced(from, count));
            if (!d->currentFrame.isEmpty()) {
                emit frameReceived(d->currentFrame);
                d->currentFrame.clear();
            }
        }
        from = to + 1;
        to = data.indexOf(BIDIB_PKT_MAGIC, from);
    }
    d->currentFrame.append(data.sliced(from));
}

QByteArray SerialTransport::escape(const QByteArray &ba)
{
    if (ba.isEmpty())
        return {};

    QByteArray result{ba};
    int i = 0;
    int o = 0;
    while (i < ba.size()) {
        quint8 c = ba[i++];
        if (c == BIDIB_PKT_MAGIC || c == BIDIB_PKT_ESCAPE) {
            result.insert(o++, BIDIB_PKT_ESCAPE);
            result[o] = c ^ 0x20;
        }
        o++;
    }
    return result;
}

tl::expected<QByteArray, Error> SerialTransport::unescape(const QByteArray &ba)
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
