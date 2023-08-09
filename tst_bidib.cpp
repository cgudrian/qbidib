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
    void addressDownstreamNonEmptyStack();
    void addressDownstreamSelf();
    void addressUpstreamSelf();
    void addressUpstreamFullStack();

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
    const auto a = Bd::address::parse({});
    QVERIFY(!a.has_value());
    QCOMPARE(a.error(), Bd::Error::OutOfData);
}

void TestBiDiB::addressParseBufferWithEmptyStack()
{
    const auto a = Bd::address::parse(ba(0));
    QVERIFY(a.has_value());
    QVERIFY(a->isMe());
    QCOMPARE(a->size(), 0);
}

void TestBiDiB::addressParseBufferWithOneEntry()
{
    const auto a = Bd::address::parse(ba(1, 0));
    QVERIFY(a.has_value());
    QVERIFY(!a->isMe());
    QCOMPARE(a->size(), 1);
}

void TestBiDiB::addressParseBufferWithFourEntries()
{
    const auto a = Bd::address::parse(ba(4, 3, 2, 1, 0));
    QVERIFY(a.has_value());
    QVERIFY(!a->isMe());
    QCOMPARE(a->size(), 4);
}

void TestBiDiB::addressParseBufferWithFiveEntries()
{
    const auto a = Bd::address::parse(ba(1, 2, 3, 4, 5, 0));
    QVERIFY(!a.has_value());
    QCOMPARE(a.error(), Bd::Error::AddressTooLong);
}

void TestBiDiB::addressDownstreamNonEmptyStack()
{
    const auto a = Bd::address(1, 2, 3, 4);
    const auto b = a.downstream();
    QVERIFY(b.has_value());
    QCOMPARE(*b, Bd::address(2, 3, 4));
}

void TestBiDiB::addressDownstreamSelf()
{
    const auto a = Bd::address();
    const auto b = a.downstream();
    QVERIFY(!b.has_value());
    QCOMPARE(b.error(), Bd::Error::AddressStackEmpty);
}

void TestBiDiB::addressUpstreamSelf()
{
    const auto a = Bd::address();
    const auto b = a.upstream(1);
    QVERIFY(b.has_value());
    QCOMPARE(*b, Bd::address::create(1));
}

void TestBiDiB::addressUpstreamFullStack()
{
    const auto a = Bd::address(2, 3, 4, 5);
    const auto b = a.upstream(1);
    QVERIFY(!b.has_value());
    QCOMPARE(b.error(), Bd::Error::AddressStackFull);
}

void TestBiDiB::messageCreateWithTypeAndPayload()
{
    const auto m = Bd::message(1, ba(1, 2, 3, 4));
    QCOMPARE(m.type(), 1);
    QCOMPARE(m.payload(), ba(1, 2, 3, 4));
}

QTEST_MAIN(TestBiDiB)

#include "tst_bidib.moc"
