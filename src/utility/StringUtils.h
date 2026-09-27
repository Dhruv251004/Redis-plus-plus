#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <cstdint>

std::string uppercase(std::string_view input);

std::optional<int64_t> parse_integer(std::string_view value);