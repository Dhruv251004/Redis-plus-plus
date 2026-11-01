#pragma once
#include <string>
#include "protocol/RespValue.h"
#include "database/Database.h"

// AOF text for a write command that just ran. Call it right after the command:
// SET ... EX and EXPIRE are logged as absolute PEXPIREAT (or DEL if the key is already gone).
std::string aof_entry_for(const RespValue &command, Database &db);
