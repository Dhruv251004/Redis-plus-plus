#include <string>
#include <cctype>
#include <optional>
using namespace std;

string uppercase(string_view input)
{
    string result(input);

    for (char &c : result)
    {
        c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    }

    return result;
}
optional<int64_t> parse_integer(string_view value)
{
    try
    {
        size_t pos = 0;

        int64_t num = stoll(string(value), &pos);

        if (pos != value.size())
            return nullopt;

        return num;
    }
    catch (const exception &)
    {
        return nullopt;
    }
}