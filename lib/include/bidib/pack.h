#pragma once

#include <bidib/error.h>

#include <QByteArray>
#include <QDebug>
#include <QString>

#include <expected.hpp>

namespace Bd {

struct Packer
{
    QByteArray ba;

    Packer() { ba.reserve(64); }

    template<class T>
    Packer &operator<<(T const &t)
    {
        ba.append(reinterpret_cast<const char *>(&t), sizeof(T));
        return *this;
    }

    Packer &operator<<(char const *s)
    {
        auto len = std::min<size_t>(255, strlen(s));
        if (len < 255) {
            ba.append(static_cast<quint8>(len));
            ba.append(s, len);
        } else {
            qWarning() << "string too long:" << s;
        }
        return *this;
    }

    Packer &operator<<(QString const &s)
    {
        quint8 len = std::min(255ll, s.length());
        ba.append(len);
        ba.append(s.toLatin1().constData(), len);
        return *this;
    }

    static QByteArray pack() { return {}; }

    template<class... Types>
    static QByteArray pack(Types const &...args)
    {
        Packer p;
        (void) (p << ... << args);
        return p.ba;
    }
};

struct Unpacker
{
    const char *buf;
    size_t avail;

    Unpacker(QByteArray const &ba)
        : buf(ba.constData())
        , avail(ba.size())
    {}

    template<typename T>
    T extract(T t, size_t bytes = sizeof(T))
    {
        buf += bytes;
        avail -= bytes;
        return t;
    }

    template<class T>
    tl::expected<T, Error> get()
    {
        return Getter<T>{}.get(*this);
    }

    template<typename T>
    tl::expected<std::tuple<T>, Error> multiget()
    {
        auto v = get<T>();
        if (v)
            return std::make_tuple(*v);
        return tl::make_unexpected(Error::OutOfData);
    }

    template<typename T1, typename T2, typename... Args>
    tl::expected<std::tuple<T1, T2, Args...>, Error> multiget()
    {
        auto v = multiget<T1>();
        if (!v)
            return tl::make_unexpected(v.error());

        auto rest = multiget<T2, Args...>();
        if (!rest)
            return tl::make_unexpected(rest.error());

        return std::tuple_cat(*v, *rest);
    }

    template<typename T, typename = int>
    struct Getter
    {
        tl::expected<T, Error> get(Unpacker &u)
        {
            if (u.avail < sizeof(T)) {
                u.avail = 0;
                return tl::make_unexpected(Error::OutOfData);
            }
            return u.extract(*reinterpret_cast<const T *>(u.buf));
        }
    };

    template<typename X>
    struct Getter<QString, X>
    {
        tl::expected<QString, Error> get(Unpacker &u)
        {
            auto len = u.get<quint8>();
            if (!len)
                return tl::make_unexpected(len.error());
            if (u.avail < *len) {
                u.avail = 0;
                return tl::make_unexpected(Error::OutOfData);
            }
            return u.extract(QString::fromLatin1(u.buf, *len), *len);
        }
    };

    template<typename T>
    struct Getter<std::optional<T>>
    {
        tl::expected<std::optional<T>, Error> get(Unpacker &u)
        {
            auto val = u.get<T>();
            if (val)
                return *val;
            return std::nullopt;
        }
    };

    template<typename... Args>
    static tl::expected<std::tuple<Args...>, Error> unpack(QByteArray const &ba)
    {
        Unpacker u(ba);
        // FIXME: There's no guarantee in what order the fold expression calls the get() method!
        //        While clang does it left-to-right (which works), GCC does it right-to-left
        //        (which breaks things).
        auto unpacked = u.multiget<Args...>();
        return unpacked;
    }

    static tl::expected<std::tuple<>, Error> unpack(QByteArray const &) { return {}; }
};

template<class... Args, class E>
static tl::expected<std::tuple<Args...>, E> unwrapExpected(
    std::tuple<tl::expected<Args, E>...> const &tuple)
{
    auto firstError = std::apply(
        [](auto const &...args) {
            std::optional<E> error{};
            (void) ((!args.has_value() ? (error = args.error(), true) : false) || ...);
            return error;
        },
        tuple);

    if (firstError.has_value())
        return tl::make_unexpected(*firstError);

    return std::apply([](auto const &...args) { return std::make_tuple(*args...); }, tuple);
}

} // namespace Bd
