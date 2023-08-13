#pragma once

#include <QtCore/QObject>

#include <bidib/message.h>

namespace Bd {

class Node : public QObject
{
    Q_OBJECT

signals:
    void messageOut(Message const &m);
public slots:
    void handleMessage(Message const &m);

public:
    Node();
};

}
