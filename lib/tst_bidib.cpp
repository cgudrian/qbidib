#include <QTest>

#include <QSignalSpy>
#include <iostream>

#include <bidib/address.h>
#include <bidib/bidib_messages.h>
#include <bidib/message.h>
#include <bidib/pack.h>
#include <bidib/serialtransport.h>

#include "QtTest/qtestcase.h"
#include "bidib/pack.h"
#include "crc.h"

class TestBiDiB : public QObject
{
    Q_OBJECT

private slots:
    void addressParseEmptyBuffer();
    void addressParseBufferWithEmptyStack();
    void addressParseBufferWithOneEntry();
    void addressParseBufferWithFourEntries();
    void addressParseBufferWithFiveEntries();
    void addressParseWithoutNullByte();
    void addressDownstream();
    void addressDownstreamSelf();
    void addressUpstream();
    void addressUpstreamFullStack();
    void addressSize();
    void addressToByteArray();

    void messageCreateWithTypeAndPayload();
    void messageToSendBuffer();

    void serialTransportProcessContiguousFrame();
    void serialTransportProcessFragmentedFrame();
    void serialTransportProcessMultipleFragmentedFrame();
    void serialTransportSkipLeadingGarbage();
    void serialTransportEscape_data();
    void serialTransportEscape();
    void serialTransportUnescape_data();
    void serialTransportUnescape();

    void computeCrc8();

    void packerPackValues();
    void packerPackStruct();
    void packerPackString();
    void packerPackNothing();

    void unpackerUnpackValues();
    void unpackerUnpackStruct();
    void unpackerUnpackString();
    void unpackerUnpackNothing();
    void unpackerUnpackOptionalValue();
    void unpackerUnpackOptionalString();
    void unpackerUnpackOutOfData();
    void unpackerTerminateAfterFirstError();
};

QByteArray ba()
{
    return {};
}

template<typename... Args>
QByteArray ba(Args... args)
{
    QByteArray b;
    b.reserve(sizeof...(Args));
    (b += ... += args);
    return b;
}

void TestBiDiB::addressParseEmptyBuffer()
{
    auto a = Bd::Address::parse({});
    QVERIFY(!a.has_value());
    QCOMPARE(a.error(), Bd::Error::OutOfData);
}

void TestBiDiB::addressParseBufferWithEmptyStack()
{
    auto a = Bd::Address::parse(ba(0));
    QVERIFY(a.has_value());
    QVERIFY(a->isLocalNode());
    QCOMPARE(a->size(), 0);
}

void TestBiDiB::addressParseBufferWithOneEntry()
{
    auto a = Bd::Address::parse(ba(1, 0));
    QVERIFY(a.has_value());
    QVERIFY(!a->isLocalNode());
    QCOMPARE(a->size(), 1);
}

void TestBiDiB::addressParseBufferWithFourEntries()
{
    auto a = Bd::Address::parse(ba(4, 3, 2, 1, 0));
    QVERIFY(a.has_value());
    QVERIFY(!a->isLocalNode());
    QCOMPARE(a->size(), 4);
}

void TestBiDiB::addressParseBufferWithFiveEntries()
{
    auto a = Bd::Address::parse(ba(1, 2, 3, 4, 5, 0));
    QVERIFY(!a.has_value());
    QCOMPARE(a.error(), Bd::Error::AddressTooLong);
}

void TestBiDiB::addressParseWithoutNullByte()
{
    auto a = Bd::Address::parse(ba(1, 2, 3));
    QVERIFY(!a.has_value());
    QCOMPARE(a.error(), Bd::Error::AddressMissingTerminator);
}

void TestBiDiB::addressDownstream()
{
    auto a = *Bd::Address::parse(ba(1, 2, 3, 4, 0));
    auto node = a.downstream();
    QVERIFY(node.has_value());
    QCOMPARE(*node, 1);
    QCOMPARE(a, *Bd::Address::parse(ba(2, 3, 4, 0)));

    node = a.downstream();
    QVERIFY(node.has_value());
    QCOMPARE(*node, 2);
    QCOMPARE(a, *Bd::Address::parse(ba(3, 4, 0)));

    node = a.downstream();
    QVERIFY(node.has_value());
    QCOMPARE(*node, 3);
    QCOMPARE(a, *Bd::Address::parse(ba(4, 0)));

    node = a.downstream();
    QVERIFY(node.has_value());
    QCOMPARE(*node, 4);
    QCOMPARE(a, Bd::Address::localNode());
}

void TestBiDiB::addressDownstreamSelf()
{
    auto a = Bd::Address::localNode();
    auto b = a.downstream();
    QVERIFY(!b.has_value());
    QCOMPARE(b.error(), Bd::Error::AddressStackEmpty);
    QCOMPARE(a, Bd::Address::localNode());
}

void TestBiDiB::addressUpstream()
{
    auto a = Bd::Address::localNode();

    auto res = a.upstream(1);
    QVERIFY(res.has_value());
    QCOMPARE(a, Bd::Address::parse(ba(1, 0)));

    res = a.upstream(2);
    QVERIFY(res.has_value());
    QCOMPARE(a, Bd::Address::parse(ba(2, 1, 0)));

    res = a.upstream(3);
    QVERIFY(res.has_value());
    QCOMPARE(a, Bd::Address::parse(ba(3, 2, 1, 0)));

    res = a.upstream(4);
    QVERIFY(res.has_value());
    QCOMPARE(a, Bd::Address::parse(ba(4, 3, 2, 1, 0)));
}

void TestBiDiB::addressUpstreamFullStack()
{
    auto a = *Bd::Address::parse(ba(2, 3, 4, 5, 0));
    auto b = a.upstream(1);
    QVERIFY(!b.has_value());
    QCOMPARE(b.error(), Bd::Error::AddressStackFull);
    QCOMPARE(a, *Bd::Address::parse(ba(2, 3, 4, 5, 0)));
}

void TestBiDiB::addressSize()
{
    QCOMPARE(Bd::Address::parse(ba(0))->size(), 0);
    QCOMPARE(Bd::Address::parse(ba(1, 0))->size(), 1);
    QCOMPARE(Bd::Address::parse(ba(1, 2, 0))->size(), 2);
    QCOMPARE(Bd::Address::parse(ba(1, 2, 3, 0))->size(), 3);
    QCOMPARE(Bd::Address::parse(ba(1, 2, 3, 4, 0))->size(), 4);
}

void TestBiDiB::addressToByteArray()
{
    auto a = *Bd::Address::parse(ba(4, 8, 4, 0));
    QCOMPARE(a.toByteArray(), ba(4, 8, 4, 0));
}

void TestBiDiB::messageCreateWithTypeAndPayload()
{
    auto m = Bd::Message(1, ba(1, 2, 3, 4));
    QCOMPARE(m.type(), 1);
    QCOMPARE(m.payload(), ba(1, 2, 3, 4));
}

void TestBiDiB::messageToSendBuffer()
{
    {
        auto m = Bd::Message(1, ba(10, 20, 30, 40));
        auto buf = m.toSendBuffer(Bd::Address::localNode(), 42);
        QVERIFY(buf.has_value());
        QCOMPARE(buf, ba(7, 0, 42, 1, 10, 20, 30, 40));
    }

    {
        auto m = Bd::Message(1, ba(10, 20, 30, 40));
        auto buf = m.toSendBuffer(*Bd::Address::parse(ba(9, 4, 5, 0)), 99);
        QVERIFY(buf.has_value());
        QCOMPARE(buf, ba(10, 9, 4, 5, 0, 99, 1, 10, 20, 30, 40));
    }

    {
        auto m = Bd::Message(1, QByteArray(100, 0));
        auto buf = m.toSendBuffer(*Bd::Address::parse(ba(9, 4, 5, 0)), 99);
        QVERIFY(!buf.has_value());
        QCOMPARE(buf.error(), Bd::Error::MessageTooLarge);
    }
}

void TestBiDiB::serialTransportProcessContiguousFrame()
{
    Bd::SerialTransport st;
    QSignalSpy sp(&st, &Bd::SerialTransport::frameReceived);
    st.processData(ba(BIDIB_PKT_MAGIC, 1, 2, 3, 4, BIDIB_PKT_MAGIC));
    QCOMPARE(sp.count(), 1);
    QCOMPARE(sp[0][0], ba(1, 2, 3, 4));
}

void TestBiDiB::serialTransportProcessFragmentedFrame()
{
    Bd::SerialTransport st;
    QSignalSpy sp(&st, &Bd::SerialTransport::frameReceived);
    st.processData(ba(BIDIB_PKT_MAGIC, 1, 2));
    QCOMPARE(sp.count(), 0);

    st.processData(ba(3, 4, BIDIB_PKT_MAGIC));
    QCOMPARE(sp.count(), 1);
    QCOMPARE(sp[0][0], ba(1, 2, 3, 4));
}

void TestBiDiB::serialTransportProcessMultipleFragmentedFrame()
{
    Bd::SerialTransport st;
    QSignalSpy sp(&st, &Bd::SerialTransport::frameReceived);
    st.processData(ba(BIDIB_PKT_MAGIC, 1, 2));
    QCOMPARE(sp.count(), 0);

    st.processData(ba(3, 4, BIDIB_PKT_MAGIC, 5, 6, BIDIB_PKT_MAGIC, 7, 8));
    QCOMPARE(sp.count(), 2);
    QCOMPARE(sp[0][0], ba(1, 2, 3, 4));
    QCOMPARE(sp[1][0], ba(5, 6));

    sp.clear();

    st.processData(ba(9, 10, BIDIB_PKT_MAGIC));
    QCOMPARE(sp.count(), 1);
    QCOMPARE(sp[0][0], ba(7, 8, 9, 10));
}

void TestBiDiB::serialTransportSkipLeadingGarbage()
{
    Bd::SerialTransport st;
    QSignalSpy sp(&st, &Bd::SerialTransport::frameReceived);
    st.processData(ba(5, 6, BIDIB_PKT_MAGIC, 1, 2, 3, 4, BIDIB_PKT_MAGIC));
    QCOMPARE(sp.count(), 1);
    QCOMPARE(sp[0][0], ba(1, 2, 3, 4));
}

void TestBiDiB::serialTransportEscape_data()
{
    QTest::addColumn<QByteArray>("escaped");
    QTest::addColumn<QByteArray>("unescaped");
    QTest::addRow("empty buffer") << QByteArray{} << QByteArray{};
    QTest::addRow("no escaping") << ba(1, 2, 3, 4) << ba(1, 2, 3, 4);

    QTest::addRow("single escape byte")
        << ba(BIDIB_PKT_ESCAPE, BIDIB_PKT_ESCAPE ^ 0x20) << ba(BIDIB_PKT_ESCAPE);

    QTest::addRow("single magic byte")
        << ba(BIDIB_PKT_ESCAPE, BIDIB_PKT_MAGIC ^ 0x20) << ba(BIDIB_PKT_MAGIC);

    QTest::addRow("multiple escaping") << ba(1,
                                             2,
                                             BIDIB_PKT_ESCAPE,
                                             BIDIB_PKT_ESCAPE ^ 0x20,
                                             3,
                                             4,
                                             BIDIB_PKT_ESCAPE,
                                             BIDIB_PKT_MAGIC ^ 0x20,
                                             5,
                                             6)
                                       << ba(1, 2, BIDIB_PKT_ESCAPE, 3, 4, BIDIB_PKT_MAGIC, 5, 6);
}

void TestBiDiB::serialTransportEscape()
{
    QFETCH(QByteArray, unescaped);
    QFETCH(QByteArray, escaped);
    QCOMPARE(Bd::SerialTransport::escape(unescaped), escaped);
}

void TestBiDiB::serialTransportUnescape_data()
{
    using Expect = tl::expected<QByteArray, Bd::Error>;

    QTest::addColumn<QByteArray>("escaped");
    QTest::addColumn<Expect>("unescaped");
    QTest::addRow("empty buffer") << QByteArray{} << Expect{QByteArray{}};
    QTest::addRow("no escaping") << ba(1, 2, 3, 4) << Expect{ba(1, 2, 3, 4)};

    QTest::addRow("single escape byte")
        << ba(BIDIB_PKT_ESCAPE, BIDIB_PKT_ESCAPE ^ 0x20) << Expect{ba(BIDIB_PKT_ESCAPE)};

    QTest::addRow("single magic byte")
        << ba(BIDIB_PKT_ESCAPE, BIDIB_PKT_MAGIC ^ 0x20) << Expect{ba(BIDIB_PKT_MAGIC)};

    QTest::addRow("multiple escaping")
        << ba(1,
              2,
              BIDIB_PKT_ESCAPE,
              BIDIB_PKT_ESCAPE ^ 0x20,
              3,
              4,
              BIDIB_PKT_ESCAPE,
              BIDIB_PKT_MAGIC ^ 0x20,
              5,
              6)
        << Expect{ba(1, 2, BIDIB_PKT_ESCAPE, 3, 4, BIDIB_PKT_MAGIC, 5, 6)};

    QTest::addRow("trailing escape byte")
        << ba(1, 2, 3, BIDIB_PKT_ESCAPE)
        << Expect{tl::make_unexpected(Bd::Error::EscapingIncomplete)};
}

void TestBiDiB::serialTransportUnescape()
{
    using T = tl::expected<QByteArray, Bd::Error>;
    QFETCH(QByteArray, escaped);
    QFETCH(T, unescaped);
    QCOMPARE(Bd::SerialTransport::unescape(escaped), unescaped);
}

void TestBiDiB::computeCrc8()
{
    QCOMPARE(Bd::computeCrc8(QByteArray::fromHex("0370dd47b501c724eabc016f747c7349")), 0x1e);
    QCOMPARE(Bd::computeCrc8(QByteArray::fromHex("0370dd47b501c724eabc016f747c73491e")), 0);
}

struct S
{
    quint8 x;
    quint8 y;

    friend bool operator==(S const &lhs, S const &rhs) { return lhs.x == rhs.x && lhs.y == rhs.y; }
};

void TestBiDiB::packerPackValues()
{
    QCOMPARE(Bd::pack(quint8(1), quint16(2), quint32(3)), ba(1, 2, 0, 3, 0, 0, 0));
}

void TestBiDiB::packerPackStruct()
{
    QCOMPARE(Bd::pack(S{42, 43}), ba(42, 43));
}

void TestBiDiB::packerPackString()
{
    QCOMPARE(Bd::pack("Hallo, Welt!"), ba(12).append("Hallo, Welt!"));
}

void TestBiDiB::packerPackNothing()
{
    QCOMPARE(Bd::pack(), ba());
}

void TestBiDiB::unpackerUnpackValues()
{
    auto t = Bd::unpack<quint8, quint16, quint32>(ba(1, 2, 0, 3, 0, 0, 0));
    QVERIFY(t.has_value());
    QCOMPARE(t, std::make_tuple(1, 2, 3));
}

void TestBiDiB::unpackerUnpackStruct()
{
    auto t = Bd::unpack<S>(ba(42, 43));
    QVERIFY(t.has_value());
    QCOMPARE(t, std::make_tuple(S{42, 43}));
}

void TestBiDiB::unpackerUnpackString()
{
    auto t1 = Bd::unpack<QString>(ba(12).append("Hallo, Welt!"));
    QVERIFY(t1.has_value());
    QCOMPARE(t1, std::make_tuple("Hallo, Welt!"));

    auto t2 = Bd::unpack<QString>(ba(10).append(QStringLiteral("Größenwahn").toLatin1()));
    QVERIFY(t2.has_value());
    QCOMPARE(t2, std::make_tuple("Größenwahn"));
}

void TestBiDiB::unpackerUnpackNothing()
{
    auto t = Bd::unpack(ba(1));
    QVERIFY(t.has_value());
    QCOMPARE(t, std::make_tuple());
}

void TestBiDiB::unpackerUnpackOptionalValue()
{
    auto t1 = Bd::unpack<quint8, std::optional<quint8>>(ba(1, 2));
    QVERIFY(t1.has_value());
    QCOMPARE(t1, std::make_tuple(1, 2));

    auto t2 = Bd::unpack<quint8, std::optional<quint8>>(ba(1));
    QVERIFY(t2.has_value());
    QCOMPARE(t2, std::make_tuple(1, std::nullopt));
}

void TestBiDiB::unpackerUnpackOptionalString()
{
    auto t1 = Bd::unpack<quint8, std::optional<QString>>(ba(1));
    QVERIFY(t1.has_value());
    QCOMPARE(t1, std::make_tuple(1, std::nullopt));

    auto t2 = Bd::unpack<quint8, std::optional<QString>>(ba(1, 12).append("Hallo, Welt!"));
    QVERIFY(t2.has_value());
    QCOMPARE(t2, std::make_tuple(1, "Hallo, Welt!"));

    auto t3 = Bd::unpack<quint8, std::optional<QString>>(ba(1, 12).append("Hallo, Welt"));
    QVERIFY(t3.has_value());
    QCOMPARE(t3, std::make_tuple(1, std::nullopt));

    auto t4 = Bd::unpack<quint8, std::optional<QString>>(ba(1, 2).append("OK"));
    QVERIFY(t4.has_value());
    QCOMPARE(t4, std::make_tuple(1, "OK"));

    auto t5 = Bd::unpack<quint8, std::optional<QString>>(
        ba(1, 10).append(QStringLiteral("Größenwahn").toLatin1()));
    QVERIFY(t5.has_value());
    QCOMPARE(t5, std::make_tuple(1, "Größenwahn"));
}

void TestBiDiB::unpackerUnpackOutOfData()
{
    auto t = Bd::unpack<quint8, quint8>(ba(1));
    QVERIFY(!t.has_value());
    QCOMPARE(t.error(), Bd::Error::OutOfData);
}

void TestBiDiB::unpackerTerminateAfterFirstError()
{
    auto t1 = Bd::unpack<quint8, std::optional<quint16>, std::optional<quint8>>(ba(1, 10));
    QVERIFY(t1.has_value());
    QCOMPARE(t1, std::make_tuple(1, std::nullopt, std::nullopt));

    auto t2 = Bd::unpack<quint8, std::optional<quint16>, std::optional<quint8>>(ba(1, 10, 4));
    QVERIFY(t2.has_value());
    QCOMPARE(t2, std::make_tuple(1, 0x040a, std::nullopt));
}

QTEST_MAIN(TestBiDiB)

#include "tst_bidib.moc"
