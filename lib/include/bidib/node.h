#pragma once

#include <QtCore/QObject>

#include <bidib/message.h>

namespace Bd {

class NodePrivate;

class Node : public QObject
{
    Q_OBJECT

signals:
    void messageToSend(Message const &msg);

public slots:
    void handleMessage(Message const &msg);

public:
    Node();

private:
    Q_DECLARE_PRIVATE(Node)
};

} // namespace Bd
