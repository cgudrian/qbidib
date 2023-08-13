#pragma once

namespace Bd {

enum class Error {
    OutOfData,
    AddressTooLong,
    AddressStackEmpty,
    AddressStackFull,
    AddressMissingTerminator,
    MessageTooLarge,
    EscapingIncomplete,
};

}
