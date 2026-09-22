#include <iostream>
#include <optional>
#include <vector>
using namespace std;

class RespValue
{
public:
    enum class Type
    {
        SimpleString,
        BulkString,
        Error,
        Integer,
        Array,
        Null
    };

    RespValue(Type, string_view);
    static RespValue simple_string(string_view);
    static RespValue bulk_string(string_view);
    static RespValue array(vector<RespValue>);
    static RespValue error(const string &);

    Type type() const;
    const string &data() const;
    const vector<RespValue> &children() const;
    bool is_error() const;

private:
    Type type_;
    string data_;
    vector<RespValue> children_;
};