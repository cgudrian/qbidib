#include <QTest>

class TestBiDiB : public QObject
{
private slots:
    void foo();
};

void TestBiDiB::foo() {}

QTEST_MAIN(TestBiDiB)
