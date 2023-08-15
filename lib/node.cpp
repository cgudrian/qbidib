#include "node.h"
#include "bidib_messages.h"

#include <QtCore/private/qobject_p.h>

namespace Bd {

static const Message NodeNA = Message::create<quint8>(MSG_NODE_NA, 0xff);
static const Message FeatureNA = Message::create<quint8>(MSG_FEATURE_NA, 0xff);

using MessageHandler = std::function<void(Message)>;

#define HANDLER2(msg, ...) [msg] = [this](auto)

#define HANDLER(msg, ...) \
    HandlerRegistration __msgReg_##msg{this, msg, &NodePrivate::__handle_##msg}; \
    void __handle_##msg(__VA_ARGS__)

class NodePrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(Node)

public:
    void handleMessage(Message const &msg)
    {
        if (auto handler = _handlers[msg.type()])
            handler(msg);
        else
            qWarning() << "unhandled message" << msg;
    }

private:
    struct Enumerator
    {
        template<typename T>
        struct Iterator
        {};

        template<typename T>
        struct Iterator<QList<T>>
        {
            using Type = QListIterator<T>;
        };

        template<typename K, typename V>
        struct Iterator<QMap<K, V>>
        {
            using Type = QMapIterator<K, V>;
        };

        template<typename T>
        struct Data
        {
            typename Iterator<T>::Type iter{};
            quint8 addr{0};
            Data(T const &container)
                : iter(container)
            {}
        };

        template<typename T>
        static QSharedPointer<Data<T>> create(T const &container)
        {
            return QSharedPointer<Data<T>>::create(container);
        }
    };

    struct HandlerRegistration
    {
        template<typename T>
        HandlerRegistration(T *node, quint8 type, void (T::*handler)())
        {
            node->_handlers[type] = [node, handler](auto) { (node->*handler)(); };
        }

        template<typename T, typename... Args>
        HandlerRegistration(T *node, quint8 type, void (T::*handler)(Args... args))
        {
            node->_handlers[type] = [node, handler](Message m) {
                auto args = Unpacker::unpack<Args...>(m.payload());
                if (args)
                    std::apply(handler, std::tuple_cat(std::make_tuple(node), *args));
                else
                    qCritical() << "error unpacking args:" << args.error() << m;
            };
        }
    };

    template<class... Types>
    void sendMessage(int type, Types const &...t)
    {
        Q_Q(Node);
        emit q->messageToSend(Message::create(type, t...));
    }

    void registerReply(quint8 id, std::function<void(Message const &msg)> func)
    {
        _handlers[id] = func;
    }

    void registerReply(quint8 id, Message const &msg)
    {
        _handlers[id] = [msg](auto) { return msg; };
    }

    QList<int> _nodes;
    quint8 _nodeTabVersion{1};
    MessageHandler _handlers[255];

    HANDLER(MSG_NODETAB_GETALL, void)
    {
        auto e = Enumerator::create(_nodes);

        sendMessage<quint8>(MSG_NODETAB_COUNT, _nodes.count());
        if (e->iter.hasNext()) {
            registerReply(MSG_NODETAB_GETNEXT, [this, e](auto) {
                sendMessage<quint8, quint8>(MSG_NODETAB, _nodeTabVersion, e->addr++, e->iter.next());
                if (!e->iter.hasNext())
                    registerReply(MSG_NODETAB_GETNEXT, NodeNA);
            });
        } else {
            registerReply(MSG_NODETAB_GETNEXT, NodeNA);
        }
    }
};

Node::Node()
    : QObject(*new NodePrivate)
{}

void Node::handleMessage(Message const &msg)
{
    Q_D(Node);
    qDebug() << "RECV" << msg;
    d->handleMessage(msg);
}

} // namespace Bd
