#include <QTest>

#include <QSignalSpy>
#include <iostream>

#include "bidib.h"
#include "bidib_messages.h"
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
    void serialTransportEscape();
    void serialTransportUnescape();

    void computeCrc8();
};

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

void TestBiDiB::serialTransportEscape()
{
    QCOMPARE(Bd::SerialTransport::escape(ba(1, 2, 3, 4)), ba(1, 2, 3, 4));
    QCOMPARE(Bd::SerialTransport::escape({}), QByteArray{});
    QCOMPARE(Bd::SerialTransport::escape(ba(BIDIB_PKT_MAGIC)),
             ba(BIDIB_PKT_ESCAPE, BIDIB_PKT_MAGIC ^ 0x20));
    QCOMPARE(Bd::SerialTransport::escape(ba(BIDIB_PKT_ESCAPE)),
             ba(BIDIB_PKT_ESCAPE, BIDIB_PKT_ESCAPE ^ 0x20));
    QCOMPARE(Bd::SerialTransport::escape(ba(1, 2, BIDIB_PKT_ESCAPE, 3, 4, BIDIB_PKT_MAGIC, 5, 6)),
             ba(1,
                2,
                BIDIB_PKT_ESCAPE,
                BIDIB_PKT_ESCAPE ^ 0x20,
                3,
                4,
                BIDIB_PKT_ESCAPE,
                BIDIB_PKT_MAGIC ^ 0x20,
                5,
                6));
}

void TestBiDiB::serialTransportUnescape()
{
    QCOMPARE(Bd::SerialTransport::unescape(ba(1, 2, 3, 4)), ba(1, 2, 3, 4));
    QCOMPARE(Bd::SerialTransport::unescape({}), QByteArray{});
    QCOMPARE(Bd::SerialTransport::unescape(ba(BIDIB_PKT_ESCAPE, BIDIB_PKT_ESCAPE ^ 0x20)),
             ba(BIDIB_PKT_ESCAPE));
    QCOMPARE(Bd::SerialTransport::unescape(ba(BIDIB_PKT_ESCAPE, BIDIB_PKT_MAGIC ^ 0x20)),
             ba(BIDIB_PKT_MAGIC));
    QCOMPARE(Bd::SerialTransport::unescape(ba(1,
                                              2,
                                              BIDIB_PKT_ESCAPE,
                                              BIDIB_PKT_ESCAPE ^ 0x20,
                                              3,
                                              4,
                                              BIDIB_PKT_ESCAPE,
                                              BIDIB_PKT_MAGIC ^ 0x20,
                                              5,
                                              6)),
             ba(1, 2, BIDIB_PKT_ESCAPE, 3, 4, BIDIB_PKT_MAGIC, 5, 6));
    QCOMPARE(Bd::SerialTransport::unescape(ba(1, 2, 3, BIDIB_PKT_ESCAPE)),
             tl::make_unexpected(Bd::Error::EscapingIncomplete));
}

void TestBiDiB::computeCrc8()
{
    QCOMPARE(Bd::computeCrc8(QByteArray::fromHex("0370dd47b501c724eabc016f747c7349")), 0x1e);
    QCOMPARE(Bd::computeCrc8(QByteArray::fromHex("0370dd47b501c724eabc016f747c73491e")), 0);
}

QTEST_MAIN(TestBiDiB)

#include "tst_bidib.moc"
