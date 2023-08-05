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

    explicit BiDiBMessage() {}

    BiDiBMessage(int type, QByteArrayView data)
        : type(type)
        , data(data.toByteArray())
    {}
};

QDebug operator<<(QDebug d, const BiDiBMessage &msg)
{
    auto addr = msg.addr.isEmpty() ? "[Self]" : qUtf8Printable(msg.addr.toHex('/'));
    d << addr << msg.num << qUtf8Printable(msgTypeToString(msg.type))
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

class BiDiBNode : public QObject
{
    Q_OBJECT

signals:
    void sendMessage(BiDiBMessage m);

public:
    explicit BiDiBNode()
    {
        _handlers[MSG_SYS_GET_MAGIC] = [this](auto) {
            emit sendMessage({MSG_SYS_MAGIC, {"\xfe\xaf", 2}});
        };

        _handlers[MSG_NODETAB_GETALL] = [this](auto) {
            emit sendMessage({MSG_NODETAB_COUNT, {"\1", 1}});
            _handlers[MSG_NODETAB_GETNEXT] = [this](auto) {
                emit sendMessage({MSG_NODETAB, {"\1\0\1\2\3\4\5\6\7", 9}});
                _handlers[MSG_NODETAB_GETNEXT] = [this](auto) {
                    emit sendMessage({MSG_NODE_NA, {"\xff", 1}});
                    _handlers[MSG_NODETAB_GETNEXT] = {};
                };
            };
        };
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
};

#include "expected.hpp"

struct Error1
{};

struct Error2
{};

tl::expected<int, Error1> func1()
{
    return 42;
}

tl::expected<QString, Error1> func2(int num)
{
    return QString::number(num);
}

int main(int argc, char *argv[])
{
    if (auto res = func1().and_then(func2))
        qDebug() << *res;

    QCoreApplication app(argc, argv);

    BiDiBSerialTransport serialTransport("/tmp/bidib-interface-B");
    BiDiBPacketParser packetParser;
    BiDiBNode node;

    QObject::connect(&serialTransport,
                     &BiDiBSerialTransport::packetReceived,
                     &packetParser,
                     &BiDiBPacketParser::parsePacket,
                     Qt::QueuedConnection);

    QObject::connect(&packetParser,
                     &BiDiBPacketParser::sendPacket,
                     &serialTransport,
                     &BiDiBSerialTransport::sendPacket,
                     Qt::QueuedConnection);

    QObject::connect(&packetParser,
                     &BiDiBPacketParser::messageReceived,
                     &node,
                     &BiDiBNode::handleMessage,
                     Qt::QueuedConnection);

    QObject::connect(&node,
                     &BiDiBNode::sendMessage,
                     &packetParser,
                     &BiDiBPacketParser::sendMessage,
                     Qt::QueuedConnection);

    return app.exec();
}

const char *MessageNames[256] = {[MSG_SYS_GET_MAGIC] = "SYS_GET_MAGIC",
                                 [MSG_SYS_MAGIC] = "SYS_MAGIC",
                                 [MSG_SYS_CLOCK] = "SYS_CLOCK",
                                 [MSG_NODETAB_GETALL] = "NODETAB_GETALL",
                                 [MSG_NODETAB_GETNEXT] = "NODETAB_GETNEXT",
                                 [MSG_NODETAB] = "NODETAB",
                                 [MSG_SYS_GET_SW_VERSION] = "SYS_GET_SW_VERSION",
                                 [MSG_FEATURE_GET] = "FEATURE_GET",
                                 [MSG_SYS_ENABLE] = "SYS_ENABLE",
                                 [MSG_STRING_GET] = "STRING_GET",
                                 [MSG_NODE_NA] = "NODE_NA",
                                 [MSG_NODETAB_COUNT] = "NODETAB_COUNT"};

inline QString msgTypeToString(quint8 type)
{
    return MessageNames[type] ? MessageNames[type] : QString::number(type);
}

#include "main.moc"
