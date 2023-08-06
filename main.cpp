#include <QByteArray>
#include <QCoreApplication>
#include <QSerialPort>

#include "bidib.h"
#include "bidib_messages.h"

#include "expected.hpp"

struct BiDiBMessage
{
    QByteArray addr{};
    quint8 num{0};
    quint8 type{0};
    QByteArray data{};
};

QDebug operator<<(QDebug d, const BiDiBMessage &msg)
{
    QString addr = msg.addr.isEmpty() ? QStringLiteral("Self") : msg.addr.toHex('/');
    d << "[" << qUtf8Printable(addr) << "]" << msg.num
      << qUtf8Printable(bidib::messageName(msg.type)) << qUtf8Printable(msg.data.toHex('-'));
    return d;
}

const quint8 crcTable[] = {0,   94,  188, 226, 97,  63,  221, 131, 194, 156, 126, 32,  163, 253,
                           31,  65,  157, 195, 33,  127, 252, 162, 64,  30,  95,  1,   227, 189,
                           62,  96,  130, 220, 35,  125, 159, 193, 66,  28,  254, 160, 225, 191,
                           93,  3,   128, 222, 60,  98,  190, 224, 2,   92,  223, 129, 99,  61,
                           124, 34,  192, 158, 29,  67,  161, 255, 70,  24,  250, 164, 39,  121,
                           155, 197, 132, 218, 56,  102, 229, 187, 89,  7,   219, 133, 103, 57,
                           186, 228, 6,   88,  25,  71,  165, 251, 120, 38,  196, 154, 101, 59,
                           217, 135, 4,   90,  184, 230, 167, 249, 27,  69,  198, 152, 122, 36,
                           248, 166, 68,  26,  153, 199, 37,  123, 58,  100, 134, 216, 91,  5,
                           231, 185, 140, 210, 48,  110, 237, 179, 81,  15,  78,  16,  242, 172,
                           47,  113, 147, 205, 17,  79,  173, 243, 112, 46,  204, 146, 211, 141,
                           111, 49,  178, 236, 14,  80,  175, 241, 19,  77,  206, 144, 114, 44,
                           109, 51,  209, 143, 12,  82,  176, 238, 50,  108, 142, 208, 83,  13,
                           239, 177, 240, 174, 76,  18,  145, 207, 45,  115, 202, 148, 118, 40,
                           171, 245, 23,  73,  8,   86,  180, 234, 105, 55,  213, 139, 87,  9,
                           235, 181, 54,  104, 138, 212, 149, 203, 41,  119, 244, 170, 72,  22,
                           233, 183, 85,  11,  136, 214, 52,  106, 43,  117, 151, 201, 74,  20,
                           246, 168, 116, 42,  200, 150, 21,  75,  169, 247, 182, 232, 10,  84,
                           215, 137, 107, 53};

struct Packer
{
    QByteArray ba;

    Packer() { ba.reserve(64); }

    template<class T>
    Packer &operator<<(const T &t)
    {
        ba.append(reinterpret_cast<const char *>(&t), sizeof(T));
        return *this;
    }

    Packer &operator<<(const char *s)
    {
        quint8 len = std::min(255ul, strlen(s));
        ba.append(len);
        ba.append(s, len);
        return *this;
    }

    Packer &operator<<(const QString &s)
    {
        quint8 len = std::min(255ll, s.length());
        ba.append(len);
        ba.append(s.toLatin1().constData(), len);
        return *this;
    }
};

struct Unpacker
{
    const char *buf;
    size_t avail;

    Unpacker(const QByteArray &ba)
        : buf(ba.constData())
        , avail(ba.size())
    {}

    auto bufferOverflow()
    {
        static auto const e = tl::make_unexpected(QStringLiteral("Buffer overflow"));
        return e;
    }

    template<typename T>
    T consume(T &&t, size_t bytes = sizeof(T))
    {
        buf += bytes;
        avail -= bytes;
        return t;
    }

    template<class T>
    tl::expected<T, QString> get()
    {
        if (avail < sizeof(T))
            return bufferOverflow();
        return consume(*reinterpret_cast<const T *>(buf));
    }

    template<>
    tl::expected<QString, QString> get()
    {
        quint8 len = *get<quint8>();
        if (avail < len)
            return bufferOverflow();
        return consume(QString::fromLatin1(buf, len), len);
    }
};

template<class... Types>
QByteArray pack(Types const &...args)
{
    Packer p;
    (p << ... << args);
    return p.ba;
}

template<class E>
struct ExpectedErrors
{
    QList<E> e;

    template<class T>
    ExpectedErrors &operator<<(const tl::expected<T, E> &t)
    {
        if (!t.has_value())
            e << t.error();
        return *this;
    }
};

template<class... Types, class E>
tl::expected<std::tuple<Types...>, E> checkUnexpected(std::tuple<tl::expected<Types, E>...> t)
{
    ExpectedErrors<E> errors;
    std::apply([&errors](auto... v) { (errors << ... << v); }, t);

    if (errors.e.isEmpty())
        return std::apply([](auto... v) { return std::make_tuple(*v...); }, t);

    return tl::unexpected(errors.e.join(", "));
}

template<typename T>
tl::expected<T, QString> unpack(const QByteArray &ba)
{
    Unpacker u(ba);
    return u.get<T>();
}

template<typename T1, class T2, class... Types>
tl::expected<std::tuple<T1, T2, Types...>, QString> unpack(const QByteArray &ba)
{
    Unpacker u(ba);
    auto t = std::make_tuple(u.get<T1>(), u.get<T2>(), u.get<Types>()...);
    return checkUnexpected(t);
}

class BiDiBSerialTransport : public QObject
{
    Q_OBJECT

public:
    explicit BiDiBSerialTransport(QString port)
        : _serial(port)
    {
        _serial.setBaudRate(QSerialPort::Baud115200);
        _serial.setDataBits(QSerialPort::Data8);
        _serial.setParity(QSerialPort::NoParity);
        _serial.setStopBits(QSerialPort::OneStop);
        if (!_serial.open(QIODevice::ReadWrite))
            qFatal("Unable to open serial port");

        connect(&_serial,
                &QSerialPort::readyRead,
                this,
                &BiDiBSerialTransport::receiveData,
                Qt::QueuedConnection);
    }

    void receiveData()
    {
        auto data = _serial.readAll();
        quint8 crc = 0;
        for (quint8 c : data) {
            switch (c) {
            case BIDIB_PKT_MAGIC:
                if (_currentPacket.isEmpty())
                    continue;

                if (!crc) {
                    // remove CRC
                    _currentPacket.removeLast();
                    emit packetReceived(_currentPacket);
                } else {
                    qWarning() << "Checksum mismatch:" << _currentPacket.toHex();
                }

                _currentPacket.clear();
                crc = 0;
                break;

            case BIDIB_PKT_ESCAPE:
                _escape = true;
                break;

            default:
                if (_escape) {
                    _escape = false;
                    c = c ^ 0x20;
                }
                crc = crcTable[crc ^ c];
                _currentPacket.append(c);
            }
        }
    }

    void sendPacket(QByteArray packet)
    {
        QByteArray out;
        out.reserve(64);
        out.append(BIDIB_PKT_MAGIC);
        int crc = 0;
        for (quint8 c : packet) {
            crc = crcTable[crc ^ c];
            if (c == BIDIB_PKT_MAGIC || c == BIDIB_PKT_ESCAPE) {
                out.append(BIDIB_PKT_ESCAPE);
                c = c ^ 0x20;
            }
            out.append(c);
        }
        out.append(crc);
        out.append(BIDIB_PKT_MAGIC);

        _serial.write(out);
    }

signals:
    void packetReceived(QByteArray packet);

private:
    QSerialPort _serial;
    QByteArray _currentPacket{};
    bool _escape{false};
};

class BiDiBPacketParser : public QObject
{
    Q_OBJECT

signals:
    void messageReceived(BiDiBMessage msg);
    void sendPacket(QByteArray packet);

public slots:
    void sendMessage(BiDiBMessage m)
    {
        m.num = nextMsgNum();
        qDebug() << "SEND" << m;

        int len = m.addr.length() + m.data.length() + 3;
        if (len > 64) {
            qCritical() << "message too large:" << m;
            return;
        }

        QByteArray packet;
        packet.reserve(len);
        packet.append(static_cast<quint8>(len));
        packet.append(m.addr);
        packet.append('\0');
        packet.append(m.num);
        packet.append(m.type);
        packet.append(m.data);

        emit sendPacket(packet);
    }

    void parsePacket(QByteArray packet)
    {
        int pos = 0;
        while (pos < packet.size()) {
            int len = packet[pos];
            QByteArrayView msgData = packet.mid(pos + 1, len);
            if (msgData.size() != len) {
                qWarning() << "expected" << len << "bytes, got" << msgData.toByteArray().toHex();
                continue;
            }

            if (auto msg = parseMsgData(msgData))
                emit messageReceived(*msg);
            else
                qWarning() << "cannot parse message data:" << msgData.toByteArray().toHex();

            pos += len + 1;
        }
    }

private:
    std::optional<BiDiBMessage> parseMsgData(QByteArrayView data)
    {
        BiDiBMessage msg;
        int i = data.indexOf(0);
        if (i == -1) {
            qWarning() << "invalid message:" << data.toByteArray().toHex();
            return {};
        }
        if (i > 4) {
            qWarning() << "invalid address:" << data.toByteArray().toHex();
            return {};
        }
        if (i > data.size() - 2) {
            qWarning() << "message too short" << data.toByteArray().toHex();
            return {};
        }
        if (i > 0)
            msg.addr = data.first(i).toByteArray();

        msg.num = data[++i];
        msg.type = data[++i];
        if (++i < data.size())
            msg.data = data.sliced(i).toByteArray();

        return msg;
    }

    inline quint8 nextMsgNum()
    {
        if (_msgNum == 0)
            _msgNum++;
        return _msgNum++;
    }

    quint8 _msgNum{};
};

using MessageHandler = std::function<void(BiDiBMessage)>;

struct __attribute__((packed)) UniqueId
{
    union {
        struct
        {
            quint8 zwitch : 1;
            quint8 booster : 1;
            quint8 accessory : 1;
            quint8 dccProg : 1;
            quint8 dccMain : 1;
            quint8 ui : 1;
            quint8 occupancy : 1;
            quint8 bridge : 1;
        } clazz;
        quint8 classId;
    };
    quint8 classIdEx;
    quint8 vendorId;
    union {
        struct
        {
            quint16 id;
            quint16 serial;
        } product;
        quint32 productId;
    };
};
static_assert(sizeof(UniqueId) == 7);

QDebug operator<<(QDebug d, const UniqueId &id)
{
    d << "VID" << qUtf8Printable(QString::number(id.vendorId, 16)) << "PID"
      << qUtf8Printable(QString::number(id.productId, 16));
    return d;
}

using namespace std::placeholders;

static quint8 Features[256] = {
    [FEATURE_CTRL_INPUT_COUNT] = 16,
    [FEATURE_CTRL_PORT_FLAT_MODEL] = 1,
    [FEATURE_FW_UPDATE_MODE] = 0,
};

struct Version
{
    quint8 patch, minor, major;
};
static_assert(sizeof(Version) == 3);

struct Time
{
    quint8 t1 : 2;
    quint8 minute : 6;
    quint8 t2 : 2;
    quint8 hour : 6;
    quint8 t3 : 2;
    quint8 dow : 6;
    quint8 t4 : 2;
    quint8 speed : 6;
};
static_assert(sizeof(Time) == 4);

class BiDiBNode : public QObject
{
    Q_OBJECT

signals:
    void sendMessage(BiDiBMessage m);

public:
    explicit BiDiBNode()
    {
        constantReply(MSG_SYS_GET_MAGIC, makeMessage<quint16>(MSG_SYS_MAGIC, BIDIB_SYS_MAGIC));
        constantReply(MSG_FEATURE_GETNEXT, makeMessage<quint8>(MSG_FEATURE_NA, 255));
        constantReply(MSG_SYS_GET_SW_VERSION, makeMessage<Version>(MSG_SYS_SW_VERSION, {1, 0, 0}));
        constantReply(MSG_BOOST_QUERY, makeMessage<quint8>(MSG_BOOST_STAT, BIDIB_BST_STATE_ON));
        constantReply(MSG_NODETAB_GETNEXT, NodeNA);

        bindMethod(MSG_NODETAB_GETALL, &BiDiBNode::nodetabGetall);
        bindMethod(MSG_FEATURE_GET, &BiDiBNode::featureGet);
        bindMethod(MSG_STRING_GET, &BiDiBNode::stringGet);
        bindMethod(MSG_SYS_CLOCK, &BiDiBNode::clock);
        bindMethod(MSG_SYS_ENABLE, &BiDiBNode::enableSystem);
        bindMethod(MSG_SYS_DISABLE, &BiDiBNode::disableSystem);
        bindMethod(MSG_LC_PORT_QUERY_ALL, &BiDiBNode::msgLcPortQueryAll);

        _nodes << UniqueId{.clazz = {.booster = 1, .accessory = 1, .dccMain = 1},
                           .vendorId = 0x0d,
                           .productId = 0xdeadbeef};
    }

public slots:
    void handleMessage(BiDiBMessage msg)
    {
        qDebug() << "RECV" << msg;
        if (auto handler = _handlers[msg.type])
            handler(msg);
        else
            qDebug() << "message not handled";
    }

private:
    static const BiDiBMessage NodeNA;

    QList<MessageHandler> _handlers{255};
    QList<UniqueId> _nodes;
    quint8 _nodeTabVersion{1};

    void nodetabGetall(BiDiBMessage)
    {
        auto iter = QSharedPointer<QListIterator<UniqueId>>::create(_nodes);

        sendReply<quint8>(MSG_NODETAB_COUNT, _nodes.count());
        if (iter->hasNext()) {
            bindLambda(MSG_NODETAB_GETNEXT, [this, iter](auto) {
                sendReply<quint8, quint8>(MSG_NODETAB, _nodeTabVersion, 0, iter->next());
                if (!iter->hasNext())
                    constantReply(MSG_NODETAB_GETNEXT, NodeNA);
            });
        } else {
            constantReply(MSG_NODETAB_GETNEXT, NodeNA);
        }
    }

    void featureGet(BiDiBMessage m)
    {
        if (auto id = unpack<quint8>(m.data))
            sendReply(MSG_FEATURE, id, Features[*id]);
    }

    void clock(BiDiBMessage m)
    {
        if (auto time = unpack<Time>(m.data))
            qDebug() << "CLOCK" << time->dow << time->hour << time->minute << time->speed;
    }

    void stringGet(BiDiBMessage m)
    {
        if (auto data = unpack<quint8, quint8>(m.data)) {
            auto [first, second] = *data;

            QString val;

            switch (first << 16 | second) {
            case 0x0000:
                val = "My Product";
                break;
            case 0x0001:
                val = "My Name";
                break;
            }

            sendReply(MSG_STRING, first, second, val);
        }
    }

    void msgLcPortQueryAll(BiDiBMessage m)
    {
        quint16 select = 0xffff;
        quint16 start = 0;
        quint16 end = 0xffff;

        switch (m.data.size()) {
        case 2:
            select = *unpack<quint16>(m.data);
            break;
        case 6:
            std::tie(select, start, end) = *unpack<quint16, quint16, quint16>(m.data);
            break;
        }

        for (quint8 port = std::max(quint16(0), start); port < std::min(quint16(15), end); ++port)
            sendReply(MSG_LC_STAT, quint8(BIDIB_PORTTYPE_SWITCH), port, quint8(0));
        sendReply(MSG_LC_NA, quint16(0xffff));
    }

    void enableSystem(BiDiBMessage m) { qDebug() << "System enabled"; }
    void disableSystem(BiDiBMessage m) { qDebug() << "System disabled"; }

    template<class T>
    void append(QByteArray &ba, const T &t)
    {
        ba.append(reinterpret_cast<const char *>(&t), sizeof(T));
    }

    template<>
    void append<QString>(QByteArray &ba, const QString &t)
    {
        assert(t.size() < 255);
        ba.append(t.size());
        ba.append(t.toLatin1(), t.size());
    }

    template<class... Types>
    void sendReply(int type, Types const &...t)
    {
        emit sendMessage(makeMessage(type, t...));
    }

    template<class... Types>
    static BiDiBMessage makeMessage(int type, Types const &...t)
    {
        BiDiBMessage m;
        m.type = type;
        m.data = pack(t...);
        return m;
    }

    template<class F>
    void bindMethod(quint8 type, F &&f)
    {
        _handlers[type] = std::bind(f, this, _1);
    }

    template<class F>
    void bindLambda(quint8 type, F &&f)
    {
        _handlers[type] = f;
    }

    void constantReply(quint8 type, const BiDiBMessage &m)
    {
        _handlers[type] = [this, m](auto) { emit sendMessage(m); };
    }

    template<quint8 id>
    inline void clearHandler()
    {
        _handlers[id] = {};
    }
};

const BiDiBMessage BiDiBNode::NodeNA = BiDiBNode::makeMessage<quint8>(MSG_NODE_NA, 0xff);

using Error1 = QString;

tl::expected<int, Error1> func1()
{
    return 42;
}

tl::expected<QString, Error1> func2(int num)
{
    if (num == 42)
        return tl::make_unexpected("Haha!");
    return QString::number(num);
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    BiDiBSerialTransport serialTransport("/tmp/bidib-interface-B");
    BiDiBPacketParser packetParser;
    BiDiBNode node;

    QObject::connect(&serialTransport,
                     &BiDiBSerialTransport::packetReceived,
                     &packetParser,
                     &BiDiBPacketParser::parsePacket);

    QObject::connect(&packetParser,
                     &BiDiBPacketParser::sendPacket,
                     &serialTransport,
                     &BiDiBSerialTransport::sendPacket);

    QObject::connect(&packetParser,
                     &BiDiBPacketParser::messageReceived,
                     &node,
                     &BiDiBNode::handleMessage);

    QObject::connect(&node, &BiDiBNode::sendMessage, &packetParser, &BiDiBPacketParser::sendMessage);

    return app.exec();
}

#include "main.moc"
