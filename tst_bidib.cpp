#include <QTest>

#include <iostream>

#include "bidib.h"

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

QTEST_MAIN(TestBiDiB)

#include "tst_bidib.moc"
