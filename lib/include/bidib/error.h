#pragma once

#include <QtCore/QObject>

namespace Bd {

Q_NAMESPACE

enum class Error {
    OutOfData,
    AddressTooLong,
    AddressStackEmpty,
    AddressStackFull,
    AddressMissingTerminator,
    MessageTooLarge,
    EscapingIncomplete,
};

Q_ENUM_NS(Error);

}
