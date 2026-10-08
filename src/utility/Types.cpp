#include "Types.h"
#include <optional>
using namespace std;

bool is_valid_value_type(uint8_t value)
{
    switch (value)
    {
    case static_cast<uint8_t>(ValueType::String):
    case static_cast<uint8_t>(ValueType::List):
    case static_cast<uint8_t>(ValueType::Set):
    case static_cast<uint8_t>(ValueType::Hash):
        return true;
    default:
        return false;
    }
}

optional<ValueType> get_value_type(uint8_t num)
{
    switch (num)
    {
    case static_cast<uint8_t>(ValueType::String):
        return ValueType::String;

    case static_cast<uint8_t>(ValueType::List):
        return ValueType::List;

    case static_cast<uint8_t>(ValueType::Set):
        return ValueType::Set;

    case static_cast<uint8_t>(ValueType::Hash):
        return ValueType::Hash;

    default:
        return nullopt;
    }
}