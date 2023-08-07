#include <QByteArray>
#include <QCoreApplication>
#include <QSerialPort>
#include <QTimer>
#include <QtCore/qobjectdefs.h>

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

struct Time
{
    quint8 minute : 6;
    quint8 t1 : 2;
    quint8 hour : 6;
    quint8 t2 : 2;
    quint8 dow : 6;
    quint8 t3 : 2;
    quint8 speed : 6;
    quint8 t4 : 2;
};
static_assert(sizeof(Time) == 4);

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
        static auto const e = tl::make_unexpected(QStringLiteral("out of data"));
        return e;
    }

    template<typename T>
    T extract(T t, size_t bytes = sizeof(T))
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
        return extract(*reinterpret_cast<const T *>(buf));
    }

    template<>
    tl::expected<QString, QString> get()
    {
        quint8 len = *get<quint8>();
        if (avail < len)
            return bufferOverflow();
        return extract(QString::fromLatin1(buf, len), len);
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

template<typename... Types>
tl::expected<std::tuple<Types...>, QString> unpack(const QByteArray &ba)
{
    Unpacker u(ba);
    return checkUnexpected(std::make_tuple(u.get<Types>()...));
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
        if (crc == BIDIB_PKT_MAGIC) {
            out.append(BIDIB_PKT_ESCAPE);
            out.append(crc ^ 0x20);
        } else {
            out.append(crc);
        }
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

typedef struct __attribute__((__packed__)) // t_bidib_cs_drive
{
    union {
        struct
        {
            uint8_t addrl; // low byte of addr
            uint8_t addrh; // high byte of addr
        };
        uint16_t addr; // true dcc address (start with 0)
    };
    uint8_t format; // BIDIB_CS_DRIVE_FORMAT_DCC14, _DCC28, _DCC128
    uint8_t active; // BIDIB_CS_DRIVE_SPEED_BIT,
        // BIDIB_CS_DRIVE_F1F4_BIT     (1<<1)
        // BIDIB_CS_DRIVE_F5F8_BIT     (1<<2)
        // BIDIB_CS_DRIVE_F9F12_BIT    (1<<3)
        // BIDIB_CS_DRIVE_F13F20_BIT   (1<<4)
        // BIDIB_CS_DRIVE_F21F28_BIT   (1<<5)
    uint8_t speed; // like DCC, MSB=1:forward, MSB=0:revers, speed=1: ESTOP
    union {
        struct
        {
            uint8_t f4_f1 : 4; // functions f4..f1
            uint8_t light : 1; // f0
            uint8_t fill : 3;  // 3 bits as usable space (ie. to store f29-f31 on a node)
        };
        uint8_t f4_f0;
    };
    union {
        struct
        {
            uint8_t f8_f5 : 4;  // functions f8..f5
            uint8_t f12_f9 : 4; // functions f12..f9
        };
        uint8_t f12_f5;
    };
    union {
        struct
        {
            uint8_t f16_f13 : 4; // functions f16..f13
            uint8_t f20_f17 : 4; // functions f20..f17
        };
        uint8_t f20_f13;
    };
    union {
        struct
        {
            uint8_t f24_f21 : 4; // functions f24..f21
            uint8_t f28_f25 : 4; // functions f28..f25
        };
        uint8_t f28_f21;
    };
} t_bidib_cs_drive;

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

class BiDiBNode : public QObject
{
    Q_OBJECT

signals:
    void sendMessage(BiDiBMessage m);

public:
    explicit BiDiBNode()
    {
        registerStaticReply(MSG_SYS_GET_MAGIC, makeMessage<quint16>(MSG_SYS_MAGIC, BIDIB_SYS_MAGIC));
        registerStaticReply(MSG_FEATURE_GETNEXT, makeMessage<quint8>(MSG_FEATURE_NA, 255));
        registerStaticReply(MSG_SYS_GET_SW_VERSION,
                            makeMessage<Version>(MSG_SYS_SW_VERSION, {1, 0, 0}));
        registerStaticReply(MSG_BOOST_QUERY,
                            makeMessage<quint8>(MSG_BOOST_STAT, BIDIB_BST_STATE_ON));
        registerStaticReply(MSG_NODETAB_GETNEXT, NodeNA);

        registerMessageHandler(MSG_STRING_GET, &BiDiBNode::stringGet);
        registerMessageHandler(MSG_SYS_ENABLE, &BiDiBNode::enableSystem);
        registerMessageHandler(MSG_SYS_DISABLE, &BiDiBNode::disableSystem);
        registerMessageHandler(MSG_LC_PORT_QUERY_ALL, &BiDiBNode::msgLcPortQueryAll);
        registerMessageHandler(MSG_NODETAB_GETALL, &BiDiBNode::nodetabGetall);
        registerMessageHandler(MSG_FEATURE_GET, &BiDiBNode::featureGet);
        registerMessageHandler(MSG_SYS_CLOCK, &BiDiBNode::clock);
        registerMessageHandler(MSG_CS_DRIVE, &BiDiBNode::handleMSG_CS_DRIVE);
        registerMessageHandler(MSG_CS_SET_STATE, &BiDiBNode::handleMSG_CS_SET_STATE);
        registerMessageHandler(MSG_ACCESSORY_SET, &BiDiBNode::handleMSG_ACCESSORY_SET);

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
    void handleMSG_CS_DRIVE(t_bidib_cs_drive cs_drive)
    {
        quint8 ack = 1;
        sendReply(MSG_CS_DRIVE_ACK, cs_drive.addr, ack);
    }

    void handleMSG_CS_SET_STATE(quint8 state)
    {
        if (state != BIDIB_CS_STATE_QUERY)
            _csState = state;
        sendReply(MSG_CS_STATE, _csState);
    }

    void handleMSG_ACCESSORY_SET(quint8 anum, quint8 aspect)
    {
        quint8 total = 2;
        quint8 execute = 0b00000011;
        quint8 wait = 10;
        sendReply(MSG_ACCESSORY_STATE, anum, aspect, total, execute, wait);
        QTimer::singleShot(1000, this, [this, anum, aspect, total] {
            quint8 execute = 0b00000010;
            quint8 wait = 0;
            sendReply(MSG_ACCESSORY_STATE, anum, aspect, total, execute, wait);
        });
    }

private:
    static const BiDiBMessage NodeNA;

    QList<MessageHandler> _handlers{255};
    QList<UniqueId> _nodes;
    quint8 _nodeTabVersion{1};
    quint8 _csState{BIDIB_CS_STATE_OFF};

    void nodetabGetall()
    {
        auto iter = QSharedPointer<QListIterator<UniqueId>>::create(_nodes);

        sendReply<quint8>(MSG_NODETAB_COUNT, _nodes.count());
        if (iter->hasNext()) {
            bindLambda(MSG_NODETAB_GETNEXT, [this, iter](auto) {
                sendReply<quint8, quint8>(MSG_NODETAB, _nodeTabVersion, 0, iter->next());
                if (!iter->hasNext())
                    registerStaticReply(MSG_NODETAB_GETNEXT, NodeNA);
            });
        } else {
            registerStaticReply(MSG_NODETAB_GETNEXT, NodeNA);
        }
    }

    void featureGet(quint8 id) { sendReply(MSG_FEATURE, id, Features[id]); }

    void clock(Time time)
    {
        qDebug() << "CLOCK" << this << time.dow << time.hour << time.minute << time.speed;
    }

    void stringGet(quint8 first, quint8 second)
    {
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

    void msgLcPortQueryAll(BiDiBMessage m)
    {
        quint16 select = 0xffff;
        quint16 start = 0;
        quint16 end = 0xffff;

        switch (m.data.size()) {
        case 2:
            std::tie(select) = *unpack<quint16>(m.data);
            break;
        case 6:
            std::tie(select, start, end) = *unpack<quint16, quint16, quint16>(m.data);
            break;
        }

        for (quint8 port = std::max(quint16(0), start); port < std::min(quint16(15), end); ++port)
            sendReply(MSG_LC_STAT, quint8(BIDIB_PORTTYPE_SWITCH), port, quint8(0));

        sendReply(MSG_LC_NA, quint16(0xffff));
    }

    void enableSystem() { qDebug() << "System enabled"; }
    void disableSystem() { qDebug() << "System disabled"; }

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

    void registerStaticReply(quint8 type, const BiDiBMessage &m)
    {
        _handlers[type] = [this, m](auto) { emit sendMessage(m); };
    }

    inline void clearHandler(quint8 type) { _handlers[type] = {}; }

    void registerMessageHandler(quint8 type, void (BiDiBNode::*handler)())
    {
        _handlers[type] = [this, handler](auto) { (this->*handler)(); };
    }

    void registerMessageHandler(quint8 type, void (BiDiBNode::*handler)(BiDiBMessage))
    {
        _handlers[type] = [this, handler](BiDiBMessage m) { (this->*handler)(m); };
    }

    template<typename... Args>
    void registerMessageHandler(quint8 type, void (BiDiBNode::*handler)(Args... args))
    {
        _handlers[type] = [this, handler](BiDiBMessage m) {
            auto args = unpack<Args...>(m.data);
            if (args)
                std::apply(handler, std::tuple_cat(std::make_tuple(this), *args));
            else
                qCritical() << "error unpacking args:" << args.error() << m;
        };
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
