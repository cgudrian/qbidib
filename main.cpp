#include <QByteArray>
#include <QCoreApplication>
#include <QRandomGenerator>
#include <QSerialPort>
#include <QTimer>
#include <QtCore/qobjectdefs.h>

#include <signal.h>

#include <bidib/bidib_messages.h>
#include <bidib/message.h>
#include <bidib/pack.h>

struct BiDiBMessage
{
    QByteArray addr{};
    quint8 num{0};
    quint8 type{0};
    QByteArray data{};
};

QDebug operator<<(QDebug d, BiDiBMessage const &msg)
{
    QString addr = msg.addr.isEmpty() ? QStringLiteral("Self") : msg.addr.toHex('/');
    d << "[" << qUtf8Printable(addr) << "]" << msg.num
      << qUtf8Printable(Bd::Message::name(msg.type)) << qUtf8Printable(msg.data.toHex('-'));
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

        connect(&_serial,
                &QSerialPort::readyRead,
                this,
                &BiDiBSerialTransport::receiveData,
                Qt::QueuedConnection);

        connect(&_serial,
                &QSerialPort::errorOccurred,
                this,
                [this](QSerialPort::SerialPortError error) {
                    qDebug() << error;
                    if (error == QSerialPort::NoError)
                        return;
                    if (_serial.isOpen())
                        _serial.close();
                    QTimer::singleShot(1000, this, [this] { _serial.open(QIODevice::ReadWrite); });
                });

        _serial.open(QIODevice::ReadWrite);
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
        out.append(static_cast<quint8>(BIDIB_PKT_MAGIC));
        int crc = 0;
        for (quint8 c : packet) {
            crc = crcTable[crc ^ c];
            if (c == BIDIB_PKT_MAGIC || c == BIDIB_PKT_ESCAPE) {
                out.append(static_cast<quint8>(BIDIB_PKT_ESCAPE));
                c = c ^ 0x20;
            }
            out.append(c);
        }
        if (crc == BIDIB_PKT_MAGIC) {
            out.append(static_cast<quint8>(BIDIB_PKT_ESCAPE));
            out.append(crc ^ 0x20);
        } else {
            out.append(crc);
        }
        out.append(static_cast<quint8>(BIDIB_PKT_MAGIC));

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

struct __attribute__((__packed__)) CsDrive
{
    quint16 addr;
    quint8 format;
    quint8 active;
    quint8 speed;
    quint8 f4_f0;
    quint8 f12_f5;
    quint8 f20_f13;
    quint8 f28_f21;
};
static_assert(sizeof(CsDrive) == 9);

struct __attribute__((packed)) UniqueId
{
    union {
        quint8 classId;
    };
    quint8 classIdEx;
    quint8 vendorId;
    quint32 productId;
};
static_assert(sizeof(UniqueId) == 7);

QDebug operator<<(QDebug d, UniqueId const &id)
{
    d << "VID" << qUtf8Printable(QString::number(id.vendorId, 16)) << "PID"
      << qUtf8Printable(QString::number(id.productId, 16));
    return d;
}

using namespace std::placeholders;

struct Version
{
    quint8 patch, minor, major;
};
static_assert(sizeof(Version) == 3);

#define HANDLE(msg, ...) \
    HandlerRegistration __reg_##msg{this, msg, &BiDiBNode::__handle_##msg}; \
    void __handle_##msg(__VA_ARGS__)

template<typename K = quint8, typename V = K>
struct KeyValue
{
    K id;
    V value;
};

using KeyValue8 = KeyValue<quint8>;
using KeyValue16 = KeyValue<quint16>;

struct HandlerRegistration
{
    template<typename T>
    HandlerRegistration(T *n, quint8 type, void (T::*handler)());

    template<typename T, typename... Args>
    HandlerRegistration(T *n, quint8 type, void (T::*handler)(Args... args));
};

struct Enumerator
{
    template<typename T>
    struct Iterator
    {};

    template<typename T>
    struct Iterator<QList<T>>
    {
        using Type = QListIterator<T>;
    };

    template<typename K, typename V>
    struct Iterator<QMap<K, V>>
    {
        using Type = QMapIterator<K, V>;
    };

    template<typename T>
    struct Data
    {
        typename Iterator<T>::Type iter{};
        quint8 addr{0};
        Data(T const &container)
            : iter(container)
        {}
    };

    template<typename T>
    static QSharedPointer<Data<T>> create(T const &container)
    {
        return QSharedPointer<Data<T>>::create(container);
    }
};

class BiDiBNode : public QObject
{
    Q_OBJECT

signals:
    void messageOut(BiDiBMessage m);

public:
    explicit BiDiBNode()
    {
        static const auto MyUniqueId = UniqueId{
            .classId = 0x0,
            .vendorId = 0x0d,
            .productId = 0xdeadbeef,
        };

        static const auto OtherUniqueId = UniqueId{
            .classId = 0x0,
            .vendorId = 0x0d,
            .productId = 0xcafebabe,
        };

        registerStaticReply(MSG_SYS_GET_MAGIC, makeMessage<quint16>(MSG_SYS_MAGIC, BIDIB_SYS_MAGIC));
        registerStaticReply(MSG_FEATURE_GETNEXT, FeatureNA);
        registerStaticReply(MSG_SYS_GET_SW_VERSION,
                            makeMessage(MSG_SYS_SW_VERSION, Version{1, 0, 0}));
        registerStaticReply(MSG_NODETAB_GETNEXT, NodeNA);
        registerStaticReply(MSG_SYS_GET_P_VERSION,
                            makeMessage(MSG_SYS_P_VERSION, quint16{BIDIB_VERSION}));
        registerStaticReply(MSG_SYS_GET_UNIQUE_ID, makeMessage(MSG_SYS_UNIQUE_ID, MyUniqueId));

        _measurementTimer.setInterval(1000);
        connect(&_measurementTimer, &QTimer::timeout, this, [this] {
            quint8 v = std::clamp<quint8>(_boosterVoltage, 0, 25) * 10;
            sendReply<KeyValue8, KeyValue8>(MSG_BOOST_DIAGNOSTIC,
                                            {BIDIB_BST_DIAG_I, 100},
                                            {BIDIB_BST_DIAG_V, v});
        });

        _nodes << MyUniqueId << OtherUniqueId;

        _features[FEATURE_BST_AMPERE] = 147;
        _features[FEATURE_BST_CURMEAS_INTERVAL] = _measurementTimer.interval() / 10;
        _features[FEATURE_BST_CUTOUT_AVAILABLE] = 1;
        _features[FEATURE_BST_CUTOUT_ON] = 1;
        _features[FEATURE_BST_INHIBIT_AUTOSTART] = 0;
        _features[FEATURE_BST_VOLT] = _boosterVoltage;
        _features[FEATURE_BST_VOLT_ADJUSTABLE] = 1;
        //        _features[FEATURE_CTRL_PORT_FLAT_MODEL] = 16;
        //        _features[FEATURE_CTRL_PORT_FLAT_MODEL_EXTENDED] = 0;
        _features[FEATURE_CTRL_SERVO_COUNT] = 16;
        _features[FEATURE_ACCESSORY_COUNT] = 16;
        _features[FEATURE_FW_UPDATE_MODE] = 0;
        _features[FEATURE_GEN_WATCHDOG] = 10;
        _features[FEATURE_STRING_SIZE] = 24;
        _features[FEATURE_STRING_NAMESPACES_AVAILABLE] = 0b101;

        _strings[0x0000] = "Roy";
        _strings[0x0001] = "Größenwahn";
    }

public slots:
    void messageIn(BiDiBMessage msg)
    {
        qDebug() << "RECV" << msg;
        if (auto handler = _handlers[msg.type])
            handler(msg);
        else
            qDebug() << "message not handled";
    }

private:
    static const BiDiBMessage NodeNA;
    static const BiDiBMessage FeatureNA;

    QList<MessageHandler> _handlers{255};
    QList<UniqueId> _nodes;
    QMap<quint8, quint8> _features;
    quint8 _boosterState{BIDIB_BST_STATE_OFF};
    quint8 _nodeTabVersion{1};
    quint8 _csState{BIDIB_CS_STATE_OFF};
    QTimer _measurementTimer{};
    quint8 _boosterVoltage{12};
    QMap<quint16, QString> _strings;

    quint8 updateFeature(quint8 id, quint8 value)
    {
        switch (id) {
        case FEATURE_BST_VOLT:
            _boosterVoltage = value = std::clamp<quint8>(value, 3, 16);
            break;

        case FEATURE_BST_CURMEAS_INTERVAL:
            value = std::max<quint8>(value, 10);
            _measurementTimer.setInterval(value * 10);
            break;

        default:
            value = _features[id];
        }

        return value;
    }

    HANDLE(MSG_NODETAB_GETALL, void)
    {
        auto e = Enumerator::create(_nodes);

        sendReply<quint8>(MSG_NODETAB_COUNT, _nodes.count());
        if (e->iter.hasNext()) {
            bindLambda(MSG_NODETAB_GETNEXT, [this, e](auto) mutable {
                sendReply<quint8, quint8>(MSG_NODETAB, _nodeTabVersion, e->addr++, e->iter.next());
                if (!e->iter.hasNext())
                    registerStaticReply(MSG_NODETAB_GETNEXT, NodeNA);
            });
        } else {
            registerStaticReply(MSG_NODETAB_GETNEXT, NodeNA);
        }
    }

    HANDLE(MSG_FEATURE_GET, quint8 id)
    {
        if (_features.contains(id))
            sendReply(MSG_FEATURE, id, _features[id]);
        else
            sendReply(MSG_FEATURE_NA, FeatureNA);
    }

    HANDLE(MSG_FEATURE_SET, quint8 id, quint8 value)
    {
        if (_features.contains(id)) {
            _features[id] = updateFeature(id, value);
            sendReply(MSG_FEATURE, id, _features[id]);
        } else {
            sendReply(MSG_FEATURE_NA, FeatureNA);
        }
    }

    HANDLE(MSG_SYS_CLOCK, Time time)
    {
        qDebug() << "CLOCK" << this << time.dow << time.hour << time.minute << time.speed;
    }

    HANDLE(MSG_LC_PORT_QUERY_ALL,
           std::optional<quint16> select,
           std::optional<quint16> start,
           std::optional<quint16> end)
    {
        for (quint16 port = std::max(quint16(0), start.value_or(0));
             port < std::min(quint16(15), end.value_or(0xffff));
             ++port)
            sendReply(MSG_LC_STAT, quint8(BIDIB_PORTTYPE_SWITCH), port, quint8(0));

        sendReply(MSG_LC_NA, quint16(0xffff));
    }

    HANDLE(MSG_LC_CONFIGX_GET_ALL, std::optional<quint16> start, std::optional<quint16> end)
    {
        auto s = start.value_or(0);
        auto e = end.value_or(0xffff);

        for (quint16 port = s; port <= e; ++port) {
            auto type = port & 0xff;
            if (type == BIDIB_PORTTYPE_SERVO || type == BIDIB_PORTTYPE_SWITCH)
                sendReply(MSG_LC_CONFIGX, port, KeyValue8{BIDIB_PCFG_SERVO_SPEED, 55});
        }
    }

    HANDLE(MSG_ACCESSORY_GET, quint8 num)
    {
        quint8 aspect = 0;
        quint8 total = 3;
        sendReply(MSG_ACCESSORY_STATE, num, aspect, total, quint8(0), quint8(0));
    }

    HANDLE(MSG_ACCESSORY_PARA_GET, quint8 anum, quint8 pnum)
    {
        sendReply(MSG_ACCESSORY_PARA, anum, BIDIB_ACCESSORY_PARA_NOTEXIST, pnum);
    }

    HANDLE(MSG_SYS_ENABLE, void) { qDebug() << "System enabled"; }

    HANDLE(MSG_SYS_DISABLE, void) { qDebug() << "System disabled"; }

    HANDLE(MSG_CS_DRIVE, CsDrive cs_drive)
    {
        quint8 ack = 1;
        sendReply(MSG_CS_DRIVE_ACK, cs_drive.addr, ack);
    }

    HANDLE(MSG_CS_SET_STATE, quint8 state)
    {
        if (state != BIDIB_CS_STATE_QUERY)
            _csState = state;
        sendReply(MSG_CS_STATE, _csState);
    }

    HANDLE(MSG_ACCESSORY_SET, quint8 anum, quint8 aspect)
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

    HANDLE(MSG_FEATURE_GETALL, std::optional<quint8> shouldStream)
    {
        // bool useStreaming = shouldStream.value_or(0) == 1;

        auto e = Enumerator::create(_features);

        sendReply<quint8>(MSG_FEATURE_COUNT, _features.count());
        if (e->iter.hasNext()) {
            bindLambda(MSG_FEATURE_GETNEXT, [this, e](auto) {
                auto feature = e->iter.next();
                sendReply<quint8, quint8>(MSG_FEATURE, feature.key(), feature.value());
                if (!e->iter.hasNext())
                    registerStaticReply(MSG_FEATURE_GETNEXT, FeatureNA);
            });
        } else {
            registerStaticReply(MSG_FEATURE_GETNEXT, FeatureNA);
        }
    }

    HANDLE(MSG_BOOST_QUERY, void) { sendReply<quint8>(MSG_BOOST_STAT, _boosterState); }

    HANDLE(MSG_BOOST_ON, quint8 local)
    {
        _boosterState = BIDIB_BST_STATE_ON;
        sendReply<quint8>(MSG_BOOST_STAT, _boosterState);
        _measurementTimer.start();
    }

    HANDLE(MSG_BOOST_OFF, quint8 local)
    {
        _boosterState = BIDIB_BST_STATE_OFF;
        sendReply<quint8>(MSG_BOOST_STAT, _boosterState);
        _measurementTimer.stop();
    }

    HANDLE(MSG_STRING_GET, quint8 ns, quint8 id)
    {
        sendReply(MSG_STRING, ns, id, _strings.value(ns << 8 | id));
    }

    HANDLE(MSG_STRING_SET, quint8 ns, quint8 id, QString s)
    {
        _strings[ns << 8 | id] = s;
        sendReply(MSG_STRING, ns, id, s);
    }

    template<class... Types>
    void sendReply(int type, Types const &...t)
    {
        emit messageOut(makeMessage(type, t...));
    }

    template<class... Types>
    static BiDiBMessage makeMessage(int type, Types const &...t)
    {
        BiDiBMessage m;
        m.type = type;
        m.data = Bd::Packer::pack(t...);
        return m;
    }

    template<class F>
    void bindLambda(quint8 type, F &&f)
    {
        _handlers[type] = f;
    }

    void registerStaticReply(quint8 type, BiDiBMessage const &m)
    {
        _handlers[type] = [this, m](auto) { emit messageOut(m); };
    }

    inline void clearHandler(quint8 type) { _handlers[type] = {}; }

    void registerMessageHandler(quint8 type, void (BiDiBNode::*handler)())
    {
        _handlers[type] = [this, handler](auto) { (this->*handler)(); };
    }

    template<typename... Args>
    void registerMessageHandler(quint8 type, void (BiDiBNode::*handler)(Args... args))
    {
        _handlers[type] = [this, handler](BiDiBMessage m) {
            auto args = Bd::Unpacker::unpack<Args...>(m.data);
            if (args)
                std::apply(handler, std::tuple_cat(std::make_tuple(this), *args));
            else
                qCritical() << "error unpacking args:" << args.error() << m;
        };
    }

    friend struct HandlerRegistration;
};

const BiDiBMessage BiDiBNode::NodeNA = BiDiBNode::makeMessage<quint8>(MSG_NODE_NA, 0xff);
const BiDiBMessage BiDiBNode::FeatureNA = BiDiBNode::makeMessage<quint8>(MSG_FEATURE_NA, 0xff);

static void terminate(int)
{
    qDebug() << "QUITTING";
    qApp->quit();
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    signal(SIGTERM, terminate);
    signal(SIGINT, terminate);

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
                     &BiDiBNode::messageIn);

    QObject::connect(&node, &BiDiBNode::messageOut, &packetParser, &BiDiBPacketParser::sendMessage);

    return app.exec();
}

template<typename T>
HandlerRegistration::HandlerRegistration(T *node, quint8 type, void (T::*handler)())
{
    node->_handlers[type] = [node, handler](auto) { (node->*handler)(); };
}

template<typename T, typename... Args>
HandlerRegistration::HandlerRegistration(T *node, quint8 type, void (T::*handler)(Args...))
{
    node->_handlers[type] = [node, handler](BiDiBMessage m) {
        auto args = Bd::Unpacker::unpack<Args...>(m.data);
        if (args)
            std::apply(handler, std::tuple_cat(std::make_tuple(node), *args));
        else
            qCritical() << "error unpacking args:" << args.error() << m;
    };
}

#include "main.moc"
