#pragma once
#include <cstdint>
#include <optional>

enum class ValueType
{
    String,
    List,
    Set,
    Hash
};

bool is_valid_value_type(uint8_t);

std::optional<ValueType> get_value_type(uint8_t);