#include <QByteArray>
#include <QCoreApplication>
#include <QSerialPort>

#include "bidib_messages.h"

QString msgTypeToString(quint8 type);

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
    d << "[" << qUtf8Printable(addr) << "]" << msg.num << qUtf8Printable(msgTypeToString(msg.type))
      << qUtf8Printable(msg.data.toHex('-'));
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

quint8 compute_crc8(const QByteArray &data)
{
    quint8 crc = 0;
    for (quint8 d : data)
        crc = crcTable[crc ^ d];
    return crc;
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

using namespace std::placeholders;

static quint8 Features[256] = {
    [FEATURE_CTRL_INPUT_COUNT] = 16,
    [FEATURE_CTRL_PORT_FLAT_MODEL] = 1,
    [FEATURE_FW_UPDATE_MODE] = 0,
};

class BiDiBNode : public QObject
{
    Q_OBJECT

signals:
    void sendMessage(BiDiBMessage m);

public:
    explicit BiDiBNode()
    {
        constantReply(MSG_SYS_GET_MAGIC, makeMessage<quint16>(MSG_SYS_MAGIC, BIDIB_SYS_MAGIC));
        constantReply(MSG_NODETAB_GETNEXT, makeMessage<quint8>(MSG_NODE_NA, 255));
        constantReply(MSG_FEATURE_GETNEXT, makeMessage<quint8>(MSG_FEATURE_NA, 255));
        constantReply(MSG_SYS_GET_SW_VERSION,
                      makeMessage<quint8, quint8, quint8>(MSG_SYS_SW_VERSION, 1, 0, 0));
        constantReply(MSG_BOOST_QUERY, makeMessage<quint8>(MSG_BOOST_STAT, BIDIB_BST_STATE_ON));

        bindMethod(MSG_NODETAB_GETALL, &BiDiBNode::nodetabGetall);
        bindMethod(MSG_FEATURE_GET, &BiDiBNode::featureGet);
        bindMethod(MSG_STRING_GET, &BiDiBNode::stringGet);
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
    QList<MessageHandler> _handlers{255};

    void nodetabGetall(BiDiBMessage)
    {
        sendReply<quint8>(MSG_NODETAB_COUNT, 1);
        sendReply<quint8, quint8, UniqueId>(MSG_NODETAB,
                                            1,
                                            0,
                                            {.clazz = {.booster = 1, .accessory = 1},
                                             .vendorId = 0x0d,
                                             .productId = 0xdeadbeef});
    }

    void featureGet(BiDiBMessage m)
    {
        quint8 id = m.data[0];
        sendReply(MSG_FEATURE, id, Features[id]);
    }

    void stringGet(BiDiBMessage m)
    {
        auto id = unpack<quint8, quint8>(m);

        QString val;

        switch (id.first << 16 | id.second) {
        case 0x0000:
            val = "My Product";
            break;
        case 0x0001:
            val = "My Name";
            break;
        }

        sendReply(MSG_STRING, id.first, id.second, val);
    }

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
        ba.append(t.toLocal8Bit(), t.size());
    }

    template<class T>
    void sendReply(int type, const T &t)
    {
        emit sendMessage(makeMessage(type, t));
    }

    template<class T1, class T2>
    void sendReply(int type, const T1 &t1, const T2 &t2)
    {
        emit sendMessage(makeMessage(type, t1, t2));
    }

    template<class T1, class T2, class T3>
    void sendReply(int type, const T1 &t1, const T2 &t2, const T3 &t3)
    {
        emit sendMessage(makeMessage(type, t1, t2, t3));
    }

    template<class T>
    BiDiBMessage makeMessage(int type, const T &t)
    {
        BiDiBMessage m;
        m.type = type;
        append(m.data, t);
        return m;
    }

    template<class T1, class T2>
    BiDiBMessage makeMessage(int type, const T1 &t1, const T2 &t2)
    {
        BiDiBMessage m;
        m.type = type;
        append(m.data, t1);
        append(m.data, t2);
        return m;
    }

    template<class T1, class T2, class T3>
    BiDiBMessage makeMessage(int type, const T1 &t1, const T2 &t2, const T3 &t3)
    {
        BiDiBMessage m;
        m.type = type;
        append(m.data, t1);
        append(m.data, t2);
        append(m.data, t3);
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

    template<class T>
    T unpack(const BiDiBMessage &msg)
    {
        return *reinterpret_cast<const T *>(msg.data.data());
    }

    template<class T1, class T2>
    std::pair<T1, T2> unpack(const BiDiBMessage &msg)
    {
        auto buf = msg.data.data();
        int pos = 0;
        T1 t1 = *reinterpret_cast<const T1 *>(&buf[pos]);
        pos += sizeof(T1);
        T2 t2 = *reinterpret_cast<const T2 *>(&buf[pos]);
        return std::make_pair(t1, t2);
    }
};

#include "expected.hpp"

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
    if (auto res = func1().and_then(func2))
        qDebug() << *res;
    else
        qDebug() << res.error();

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

    QObject::connect(&node,
                     &BiDiBNode::sendMessage,
                     &packetParser,
                     &BiDiBPacketParser::sendMessage);

    return app.exec();
}

#define MSG(msg) [MSG_##msg] = #msg

const char *MessageNames[256] = {MSG(SYS_GET_MAGIC),      MSG(SYS_MAGIC),       MSG(SYS_CLOCK),
                                 MSG(NODETAB_GETALL),     MSG(NODETAB_GETNEXT), MSG(NODETAB),
                                 MSG(SYS_GET_SW_VERSION), MSG(SYS_SW_VERSION),  MSG(FEATURE_GETALL),
                                 MSG(FEATURE_GETNEXT),    MSG(FEATURE_GET),     MSG(FEATURE),
                                 MSG(FEATURE_NA),         MSG(SYS_ENABLE),      MSG(NODE_NA),
                                 MSG(STRING_GET),         MSG(STRING_SET),      MSG(STRING),
                                 MSG(SYS_DISABLE),        MSG(BOOST_QUERY),     MSG(BOOST_STAT),
                                 MSG(NODETAB_COUNT)};

inline QString msgTypeToString(quint8 type)
{
    return MessageNames[type] ? MessageNames[type] : QString::number(type);
}

#include "main.moc"
