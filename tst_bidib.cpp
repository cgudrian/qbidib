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

    void messageCreateWithTypeAndPayload();
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
    const auto a = Bd::Address::parse({});
    QVERIFY(!a.has_value());
    QCOMPARE(a.error(), Bd::Error::OutOfData);
}

void TestBiDiB::addressParseBufferWithEmptyStack()
{
    const auto a = Bd::Address::parse(ba(0));
    QVERIFY(a.has_value());
    QVERIFY(a->isLocalNode());
    QCOMPARE(a->size(), 0);
}

void TestBiDiB::addressParseBufferWithOneEntry()
{
    const auto a = Bd::Address::parse(ba(1, 0));
    QVERIFY(a.has_value());
    QVERIFY(!a->isLocalNode());
    QCOMPARE(a->size(), 1);
}

void TestBiDiB::addressParseBufferWithFourEntries()
{
    const auto a = Bd::Address::parse(ba(4, 3, 2, 1, 0));
    QVERIFY(a.has_value());
    QVERIFY(!a->isLocalNode());
    QCOMPARE(a->size(), 4);
}

void TestBiDiB::addressParseBufferWithFiveEntries()
{
    const auto a = Bd::Address::parse(ba(1, 2, 3, 4, 5, 0));
    QVERIFY(!a.has_value());
    QCOMPARE(a.error(), Bd::Error::AddressTooLong);
}

void TestBiDiB::addressParseWithoutNullByte()
{
    const auto a = Bd::Address::parse(ba(1, 2, 3));
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
    const auto b = a.downstream();
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
    const auto b = a.upstream(1);
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

void TestBiDiB::messageCreateWithTypeAndPayload()
{
    const auto m = Bd::Message(1, ba(1, 2, 3, 4));
    QCOMPARE(m.type(), 1);
    QCOMPARE(m.payload(), ba(1, 2, 3, 4));
}

QTEST_MAIN(TestBiDiB)

#include "tst_bidib.moc"
