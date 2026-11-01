#include "AofEntry.h"
#include "protocol/RespSerializer.h"
#include "utility/StringUtils.h"
using namespace std;

static string serialize_command(initializer_list<RespValue> parts)
{
    return RespSerializer::serialize(RespValue::array(vector<RespValue>(parts)));
}

string aof_entry_for(const RespValue &command, Database &db)
{
    const auto &args = command.children();
    string name = uppercase(args[0].data());

    bool has_relative_time = name == "EXPIRE" || (name == "SET" && args.size() > 3);
    if (!has_relative_time)
        return RespSerializer::serialize(command);

    const RespValue &key = args[1];

    // ttl() also drops a key that has already expired.
    auto deadline = db.ttl(key.data());
    if (!deadline.has_value())
        return serialize_command({RespValue::bulk_string("DEL"), key});

    string entry;
    if (name == "SET")
        entry = serialize_command({RespValue::bulk_string("SET"), key, args[2]});
    if (*deadline != NO_EXPIRY)
        entry += serialize_command({RespValue::bulk_string("PEXPIREAT"), key,
                                    RespValue::bulk_string(to_string(*deadline))});
    return entry;
}
