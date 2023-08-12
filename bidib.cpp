#include "bidib.h"
#include "bidib_messages.h"

namespace Bd {

static constexpr auto MAX_MESSAGE_SIZE = 63;

#define MSG(msg) [msg] = #msg

static const char *MessageNames[256] = {
    MSG(MSG_ACCESSORY_GET),
    MSG(MSG_ACCESSORY_GETALL),
    MSG(MSG_ACCESSORY_NOTIFY),
    MSG(MSG_ACCESSORY_PARA),
    MSG(MSG_ACCESSORY_PARA_GET),
    MSG(MSG_ACCESSORY_PARA_SET),
    MSG(MSG_ACCESSORY_SET),
    MSG(MSG_ACCESSORY_STATE),
    MSG(MSG_BM_ACCESSORY),
    MSG(MSG_BM_ADDRESS),
    MSG(MSG_BM_ADDR_GET_RANGE),
    MSG(MSG_BM_CONFIDENCE),
    MSG(MSG_BM_CURRENT),
    MSG(MSG_BM_CV),
    MSG(MSG_BM_DYN_STATE),
    MSG(MSG_BM_FREE),
    MSG(MSG_BM_GET_CONFIDENCE),
    MSG(MSG_BM_GET_RANGE),
    MSG(MSG_BM_MIRROR_FREE),
    MSG(MSG_BM_MIRROR_MULTIPLE),
    MSG(MSG_BM_MIRROR_OCC),
    MSG(MSG_BM_MIRROR_POSITION),
    MSG(MSG_BM_MULTIPLE),
    MSG(MSG_BM_OCC),
    MSG(MSG_BM_POSITION),
    MSG(MSG_BM_RCPLUS),
    MSG(MSG_BM_SPEED),
    MSG(MSG_BM_XPOM),
    MSG(MSG_BOOST_CURRENT),
    MSG(MSG_BOOST_DIAGNOSTIC),
    MSG(MSG_BOOST_OFF),
    MSG(MSG_BOOST_ON),
    MSG(MSG_BOOST_QUERY),
    MSG(MSG_BOOST_STAT),
    MSG(MSG_CS_ACCESSORY),
    MSG(MSG_CS_ACCESSORY_ACK),
    MSG(MSG_CS_ACCESSORY_MANUAL),
    MSG(MSG_CS_ALLOCATE),
    MSG(MSG_CS_ALLOC_ACK),
    MSG(MSG_CS_BIN_STATE),
    MSG(MSG_CS_DCCA),
    MSG(MSG_CS_DCCA_ACK),
    MSG(MSG_CS_DRIVE),
    MSG(MSG_CS_DRIVE_ACK),
    MSG(MSG_CS_DRIVE_EVENT),
    MSG(MSG_CS_DRIVE_MANUAL),
    MSG(MSG_CS_DRIVE_STATE),
    MSG(MSG_CS_POM),
    MSG(MSG_CS_POM_ACK),
    MSG(MSG_CS_PROG),
    MSG(MSG_CS_PROG_STATE),
    MSG(MSG_CS_QUERY),
    MSG(MSG_CS_RCPLUS),
    MSG(MSG_CS_RCPLUS_ACK),
    MSG(MSG_CS_SET_STATE),
    MSG(MSG_CS_STATE),
    MSG(MSG_DDIS),
    MSG(MSG_DSYS),
    MSG(MSG_FEATURE),
    MSG(MSG_FEATURE_COUNT),
    MSG(MSG_FEATURE_GET),
    MSG(MSG_FEATURE_GETALL),
    MSG(MSG_FEATURE_GETNEXT),
    MSG(MSG_FEATURE_NA),
    MSG(MSG_FEATURE_SET),
    MSG(MSG_FW_UPDATE_OP),
    MSG(MSG_FW_UPDATE_STAT),
    MSG(MSG_GET_PKT_CAPACITY),
    MSG(MSG_LC_CONFIG),
    MSG(MSG_LC_CONFIGX),
    MSG(MSG_LC_CONFIGX_GET),
    MSG(MSG_LC_CONFIGX_GET_ALL),
    MSG(MSG_LC_CONFIGX_SET),
    MSG(MSG_LC_CONFIG_GET),
    MSG(MSG_LC_CONFIG_SET),
    MSG(MSG_LC_KEY),
    MSG(MSG_LC_KEY_QUERY),
    MSG(MSG_LC_MACRO),
    MSG(MSG_LC_MACRO_GET),
    MSG(MSG_LC_MACRO_HANDLE),
    MSG(MSG_LC_MACRO_PARA),
    MSG(MSG_LC_MACRO_PARA_GET),
    MSG(MSG_LC_MACRO_PARA_SET),
    MSG(MSG_LC_MACRO_SET),
    MSG(MSG_LC_MACRO_STATE),
    MSG(MSG_LC_NA),
    MSG(MSG_LC_OUTPUT),
    MSG(MSG_LC_PORT_QUERY),
    MSG(MSG_LC_PORT_QUERY_ALL),
    MSG(MSG_LC_STAT),
    MSG(MSG_LC_WAIT),
    MSG(MSG_LOCAL_ACCESSORY),
    MSG(MSG_LOCAL_ANNOUNCE),
    MSG(MSG_LOCAL_BIDIB_DOWN),
    MSG(MSG_LOCAL_BIDIB_UP),
    MSG(MSG_LOCAL_DISCOVER),
    MSG(MSG_LOCAL_LOGOFF),
    MSG(MSG_LOCAL_LOGON),
    MSG(MSG_LOCAL_LOGON_ACK),
    MSG(MSG_LOCAL_LOGON_REJECTED),
    MSG(MSG_LOCAL_PING),
    MSG(MSG_LOCAL_PONG),
    MSG(MSG_LOCAL_SYNC),
    MSG(MSG_NODETAB),
    MSG(MSG_NODETAB_COUNT),
    MSG(MSG_NODETAB_GETALL),
    MSG(MSG_NODETAB_GETNEXT),
    MSG(MSG_NODE_CHANGED_ACK),
    MSG(MSG_NODE_LOST),
    MSG(MSG_NODE_NA),
    MSG(MSG_NODE_NEW),
    MSG(MSG_PKT_CAPACITY),
    MSG(MSG_STALL),
    MSG(MSG_STRING),
    MSG(MSG_STRING_GET),
    MSG(MSG_STRING_SET),
    MSG(MSG_SYS_CLOCK),
    MSG(MSG_SYS_DISABLE),
    MSG(MSG_SYS_ENABLE),
    MSG(MSG_SYS_ERROR),
    MSG(MSG_SYS_GET_ERROR),
    MSG(MSG_SYS_GET_MAGIC),
    MSG(MSG_SYS_GET_P_VERSION),
    MSG(MSG_SYS_GET_SW_VERSION),
    MSG(MSG_SYS_GET_UNIQUE_ID),
    MSG(MSG_SYS_IDENTIFY),
    MSG(MSG_SYS_IDENTIFY_STATE),
    MSG(MSG_SYS_MAGIC),
    MSG(MSG_SYS_PING),
    MSG(MSG_SYS_PONG),
    MSG(MSG_SYS_P_VERSION),
    MSG(MSG_SYS_RESET),
    MSG(MSG_SYS_SW_VERSION),
    MSG(MSG_SYS_UNIQUE_ID),
    MSG(MSG_VENDOR),
    MSG(MSG_VENDOR_ACK),
    MSG(MSG_VENDOR_DISABLE),
    MSG(MSG_VENDOR_ENABLE),
    MSG(MSG_VENDOR_GET),
    MSG(MSG_VENDOR_SET),
};

QString messageName(quint8 type)
{
    return MessageNames[type] ? MessageNames[type] : QString::number(type);
}

QDebug operator<<(QDebug d, const Address &a)
{
    d << QByteArray(reinterpret_cast<const char *>(&a), sizeof(a)).toHex('-');
    return d;
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

SerialConnection::SerialConnection(const QString &port)
    : _serial(port)
{
    connect(&_serial, &QSerialPort::readyRead, this, &SerialConnection::readData);
}

void SerialConnection::readData()
{
    auto data = _serial.readAll();
    if (!data.isEmpty())
        emit dataReceived(data);
}

void SerialConnection::sendData(const QByteArray &data)
{
    _serial.write(data);
}

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
