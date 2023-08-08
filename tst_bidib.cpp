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
    const auto a = bdb::address::parse({});
    QVERIFY(!a.has_value());
    QCOMPARE(a.error(), bdb::error::out_of_data);
}

void TestBiDiB::addressParseBufferWithEmptyStack()
{
    const auto a = bdb::address::parse(ba(0));
    QVERIFY(a.has_value());
    QVERIFY(a->isMe());
    QCOMPARE(a->size(), 0);
}

void TestBiDiB::addressParseBufferWithOneEntry()
{
    const auto a = bdb::address::parse(ba(1, 0));
    QVERIFY(a.has_value());
    QVERIFY(!a->isMe());
    QCOMPARE(a->size(), 1);
}

void TestBiDiB::addressParseBufferWithFourEntries()
{
    const auto a = bdb::address::parse(ba(4, 3, 2, 1, 0));
    QVERIFY(a.has_value());
    QVERIFY(!a->isMe());
    QCOMPARE(a->size(), 4);
}

void TestBiDiB::addressParseBufferWithFiveEntries()
{
    const auto a = bdb::address::parse(ba(1, 2, 3, 4, 5, 0));
    QVERIFY(!a.has_value());
    QCOMPARE(a.error(), bdb::error::address_too_large);
}

void TestBiDiB::addressDownstreamNonEmptyStack()
{
    const auto a = bdb::address(1, 2, 3, 4);
    const auto b = a.downstream();
    QVERIFY(b.has_value());
    QCOMPARE(*b, bdb::address(2, 3, 4));
}

void TestBiDiB::addressDownstreamSelf()
{
    const auto a = bdb::address();
    const auto b = a.downstream();
    QVERIFY(!b.has_value());
    QCOMPARE(b.error(), bdb::error::address_stack_empty);
}

void TestBiDiB::addressUpstreamSelf()
{
    const auto a = bdb::address();
    const auto b = a.upstream(1);
    QVERIFY(b.has_value());
    QCOMPARE(*b, bdb::address::create(1));
}

void TestBiDiB::addressUpstreamFullStack()
{
    const auto a = bdb::address(2, 3, 4, 5);
    const auto b = a.upstream(1);
    QVERIFY(!b.has_value());
    QCOMPARE(b.error(), bdb::error::address_stack_full);
}

void TestBiDiB::messageCreateWithTypeAndPayload()
{
    const auto m = bdb::message(1, ba(1, 2, 3, 4));
    QCOMPARE(m.type(), 1);
    QCOMPARE(m.payload(), ba(1, 2, 3, 4));
}

QTEST_MAIN(TestBiDiB)

#include "tst_bidib.moc"
