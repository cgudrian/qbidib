#include "node.h"

#include <QtCore/private/qobject_p.h>

namespace Bd {

class NodePrivate : public QObjectPrivate
{};

void Node::handleMessage(Message const &m) {}

Node::Node()
    : QObject(*new NodePrivate)
{}

} // namespace Bd
