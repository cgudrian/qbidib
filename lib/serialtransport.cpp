#include <bidib/serialtransport.h>
#include <bidib/bidib_messages.h>

namespace Bd {

void SerialTransport::processData(const QByteArray &data)
{
    auto from = 0;
    auto to = data.indexOf(BIDIB_PKT_MAGIC, from);

    // skip leading garbage
    if (_currentFrame.isEmpty() && to > 0)
        from = to;

    while (to >= 0) {
        auto count = to - from;
        if (count) {
            _currentFrame.append(data.sliced(from, count));
            if (!_currentFrame.isEmpty()) {
                emit frameReceived(_currentFrame);
                _currentFrame.clear();
            }
        }
        from = to + 1;
        to = data.indexOf(BIDIB_PKT_MAGIC, from);
    }
    _currentFrame.append(data.sliced(from));
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
