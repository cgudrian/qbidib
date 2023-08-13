#pragma once

#include <bidib/error.h>

#include <QByteArray>
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
        quint8 len = std::min(255ul, strlen(s));
        ba.append(len);
        ba.append(s, len);
        return *this;
    }

    Packer &operator<<(QString const &s)
    {
        quint8 len = std::min(255ll, s.length());
        ba.append(len);
        ba.append(s.toLatin1().constData(), len);
        return *this;
    }

    static QByteArray pack()
    {
        return {};
    }

    template<class... Types>
    static QByteArray pack(Types const &...args)
    {
        Packer p;
        (void)(p << ... << args);
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

    template<>
    struct Getter<QString>
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
        auto unpacked = std::make_tuple(u.get<Args>()...);
        return unwrapExpected(unpacked);
    }

    static tl::expected<std::tuple<>, Error> unpack(QByteArray const &)
    {
        return {};
    }
};

template<class... Args, class E>
static tl::expected<std::tuple<Args...>, E> unwrapExpected(std::tuple<tl::expected<Args, E>...> const &tuple)
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
