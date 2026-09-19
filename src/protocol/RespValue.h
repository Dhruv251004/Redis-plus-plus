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
    static RespValue SimpleString(string_view);
    static RespValue BulkString(string_view);
    static RespValue Array(vector<RespValue>);
    static RespValue Error(const string &);

    Type type() const;
    const string &data() const;
    const vector<RespValue> &Children() const;
    bool isError() const;

private:
    Type type_;
    string data_;
    vector<RespValue> children_;
};