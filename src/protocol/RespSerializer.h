#include "RespValue.h"

class RespSerializer
{
public:
    static string serialize(const RespValue &);
    static string serialize_simple_string(std::string_view);
    static string serialize_bulk_string(std::string_view);
    static string serialize_error(std::string_view);
    static string serialize_null_bulk_string();
};