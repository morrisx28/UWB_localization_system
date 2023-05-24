#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <limits>
#include <cinttypes>
#include <iostream>
#include "TinyJson.h"

ROOT_NAMESPACE_DECLARE

namespace core
{

static const int max_depth = 200;

/* Helper for representing null - just a do-nothing struct, plus comparison
 * operators so the helpers in JsonValue work. We can't use nullptr_t because
 * it may not be orderable.
 */
struct NullStruct
{
    bool operator==(NullStruct) const
    {
        return true;
    }
    bool operator<(NullStruct) const
    {
        return false;
    }
};

/* * * * * * * * * * * * * * * * * * * *
 * Serialization
 */

static void dumpJson(NullStruct, std::string& out)
{
    out += "null";
}

static void dumpJson(double value, std::string& out)
{
    if (std::isfinite(value))
    {
        char buf[32];
        snprintf(buf, sizeof buf, "%.17g", value);
        out += buf;
    }
    else
    {
        out += "null";
    }
}

static void dumpJson(int64_t value, std::string& out)
{
    char buf[32];
    snprintf(buf, sizeof buf, "%" PRId64, value);
    out += buf;
}

static void dumpJson(bool value, std::string& out)
{
    out += value ? "true" : "false";
}

static void dumpJson(const std::string& value, std::string& out)
{
    out += '"';
    for (size_t i = 0; i < value.length(); i++)
    {
        const char ch = value[i];
        if (ch == '\\')
        {
            out += "\\\\";
        }
        else if (ch == '"')
        {
            out += "\\\"";
        }
        else if (ch == '\b')
        {
            out += "\\b";
        }
        else if (ch == '\f')
        {
            out += "\\f";
        }
        else if (ch == '\n')
        {
            out += "\\n";
        }
        else if (ch == '\r')
        {
            out += "\\r";
        }
        else if (ch == '\t')
        {
            out += "\\t";
        }
        else if (static_cast<uint8_t>(ch) <= 0x1f)
        {
            char buf[8];
            snprintf(buf, sizeof buf, "\\u%04x", ch);
            out += buf;
        }
        else if (static_cast<uint8_t>(ch) == 0xe2
            && static_cast<uint8_t>(value[i + 1]) == 0x80
            && static_cast<uint8_t>(value[i + 2]) == 0xa8)
        {
            out += "\\u2028";
            i += 2;
        }
        else if (static_cast<uint8_t>(ch) == 0xe2
            && static_cast<uint8_t>(value[i + 1]) == 0x80
            && static_cast<uint8_t>(value[i + 2]) == 0xa9)
        {
            out += "\\u2029";
            i += 2;
        }
        else
        {
            out += ch;
        }
    }
    out += '"';
}

static void dumpJson(const Json::Array& values, std::string& out)
{
    bool first = true;
    out += "[";
    for (const auto& value : values)
    {
        if (!first)
            out += ", ";
        value.dump(out);
        first = false;
    }
    out += "]";
}

static void dumpJson(const Json::Object& values, std::string& out)
{
    bool first = true;
    out += "{";
    for (const auto& kv : values)
    {
        if (!first)
            out += ", ";
        dumpJson(kv.first, out);
        out += ": ";
        kv.second.dump(out);
        first = false;
    }
    out += "}";
}

void Json::dump(std::string& out) const
{
    m_ptr->dump(out);
}

/* * * * * * * * * * * * * * * * * * * *
 * Value wrappers
 */

template <Json::Type tag, typename T>
class Value : public JsonValue
{
protected:
    // Constructors
    explicit Value(const T& value)
        : m_value(value)
    { }
    explicit Value(T&& value)
        : m_value(std::move(value))
    { }

    // Get type tag
    Json::Type type() const override
    {
        return tag;
    }

    // Comparisons
    bool equals(const JsonValue* other) const override
    {
        return m_value == static_cast<const Value<tag, T>*>(other)->m_value;
    }
    bool less(const JsonValue* other) const override
    {
        return m_value < static_cast<const Value<tag, T>*>(other)->m_value;
    }

    const T m_value;
    void dump(std::string& out) const override
    {
        acfw::core::dumpJson(m_value, out);
    }
};

class JsonDouble final : public Value<Json::NUMBER, double>
{
    double numberValue() const override
    {
        return m_value;
    }
    int64_t intValue() const override
    {
        return static_cast<int64_t>(m_value);
    }
    bool equals(const JsonValue* other) const override
    {
        return m_value == other->numberValue();
    }
    bool less(const JsonValue* other) const override
    {
        return m_value < other->numberValue();
    }

public:
    explicit JsonDouble(double value)
        : Value(value)
    { }
};

class JsonInt final : public Value<Json::NUMBER, int64_t>
{
    double numberValue() const override
    {
        return m_value;
    }
    int64_t intValue() const override
    {
        return m_value;
    }
    bool equals(const JsonValue* other) const override
    {
        return m_value == other->numberValue();
    }
    bool less(const JsonValue* other) const override
    {
        return m_value < other->numberValue();
    }

public:
    explicit JsonInt(int64_t value)
        : Value(value)
    { }
};

class JsonBoolean final : public Value<Json::BOOL, bool>
{
    bool boolValue() const override
    {
        return m_value;
    }

public:
    explicit JsonBoolean(bool value)
        : Value(value)
    { }
};

class JsonString final : public Value<Json::STRING, std::string>
{
    const std::string& stringValue() const override
    {
        return m_value;
    }

public:
    explicit JsonString(const std::string& value)
        : Value(value)
    { }
    explicit JsonString(std::string&& value)
        : Value(move(value))
    { }
};

class JsonArray final : public Value<Json::ARRAY, Json::Array>
{
    const Json::Array& arrayItems() const override
    {
        return m_value;
    }
    const Json& operator[](size_t i) const override;

public:
    explicit JsonArray(const Json::Array& value)
        : Value(value)
    { }
    explicit JsonArray(Json::Array&& value)
        : Value(move(value))
    { }
};

class JsonObject final : public Value<Json::OBJECT, Json::Object>
{
    const Json::Object& objectItems() const override
    {
        return m_value;
    }
    const Json& operator[](const std::string& key) const override;

public:
    explicit JsonObject(const Json::Object& value)
        : Value(value)
    { }
    explicit JsonObject(Json::Object&& value)
        : Value(move(value))
    { }
};

class JsonNull final : public Value<Json::NUL, NullStruct>
{
public:
    JsonNull()
        : Value({})
    { }
};

/* * * * * * * * * * * * * * * * * * * *
 * Static globals - static-init-safe
 */
struct Statics
{
    const std::shared_ptr<JsonValue> null = std::make_shared<JsonNull>();
    const std::shared_ptr<JsonValue> t    = std::make_shared<JsonBoolean>(true);
    const std::shared_ptr<JsonValue> f = std::make_shared<JsonBoolean>(false);
    const std::string emptyString;
    const std::vector<Json> emptyVector;
    const std::map<std::string, Json> emptyMap;
    Statics()
    { }
};

static const Statics& statics()
{
    static const Statics s{};
    return s;
}

static const Json& staticNull()
{
    // This has to be separate, not in Statics, because Json() accesses
    // statics().null.
    static const Json jsonNull;
    return jsonNull;
}

/* * * * * * * * * * * * * * * * * * * *
 * Constructors
 */

Json::Json() noexcept
    : m_ptr(statics().null)
{ }
Json::Json(std::nullptr_t) noexcept
    : m_ptr(statics().null)
{ }
Json::Json(double value)
    : m_ptr(std::make_shared<JsonDouble>(value))
{ }
Json::Json(int64_t value)
    : m_ptr(std::make_shared<JsonInt>(value))
{ }
Json::Json(int value)
    : m_ptr(std::make_shared<JsonInt>(value))
{ }
Json::Json(bool value)
    : m_ptr(value ? statics().t : statics().f)
{ }
Json::Json(const std::string& value)
    : m_ptr(std::make_shared<JsonString>(value))
{ }
Json::Json(std::string&& value)
    : m_ptr(std::make_shared<JsonString>(move(value)))
{ }
Json::Json(const char* value)
    : m_ptr(std::make_shared<JsonString>(value))
{ }
Json::Json(const Json::Array& values)
    : m_ptr(std::make_shared<JsonArray>(values))
{ }
Json::Json(Json::Array&& values)
    : m_ptr(std::make_shared<JsonArray>(move(values)))
{ }
Json::Json(const Json::Object& values)
    : m_ptr(std::make_shared<JsonObject>(values))
{ }
Json::Json(Json::Object&& values)
    : m_ptr(std::make_shared<JsonObject>(move(values)))
{ }

/* * * * * * * * * * * * * * * * * * * *
 * Accessors
 */

Json::Type Json::type() const
{
    return m_ptr->type();
}
double Json::numberValue() const
{
    return m_ptr->numberValue();
}
int64_t Json::intValue() const
{
    return m_ptr->intValue();
}
bool Json::boolValue() const
{
    return m_ptr->boolValue();
}
const std::string& Json::stringValue() const
{
    return m_ptr->stringValue();
}
const std::vector<Json>& Json::arrayItems() const
{
    return m_ptr->arrayItems();
}
const std::map<std::string, Json>& Json::objectItems() const
{
    return m_ptr->objectItems();
}
const Json& Json::operator[](size_t i) const
{
    return (*m_ptr)[i];
}
const Json& Json::operator[](const std::string& key) const
{
    return (*m_ptr)[key];
}

double JsonValue::numberValue() const
{
    return 0;
}
int64_t JsonValue::intValue() const
{
    return 0;
}
bool JsonValue::boolValue() const
{
    return false;
}
const std::string& JsonValue::stringValue() const
{
    return statics().emptyString;
}
const std::vector<Json>& JsonValue::arrayItems() const
{
    return statics().emptyVector;
}
const std::map<std::string, Json>& JsonValue::objectItems() const
{
    return statics().emptyMap;
}
const Json& JsonValue::operator[](size_t) const
{
    return staticNull();
}
const Json& JsonValue::operator[](const std::string&) const
{
    return staticNull();
}

const Json& JsonObject::operator[](const std::string& key) const
{
    auto iter = m_value.find(key);
    return (iter == m_value.end()) ? staticNull() : iter->second;
}
const Json& JsonArray::operator[](size_t i) const
{
    if (i >= m_value.size())
        return staticNull();
    else
        return m_value[i];
}

/* * * * * * * * * * * * * * * * * * * *
 * Comparison
 */

bool Json::operator==(const Json& other) const
{
    if (m_ptr == other.m_ptr)
        return true;
    if (m_ptr->type() != other.m_ptr->type())
        return false;

    return m_ptr->equals(other.m_ptr.get());
}

bool Json::operator<(const Json& other) const
{
    if (m_ptr == other.m_ptr)
        return false;
    if (m_ptr->type() != other.m_ptr->type())
        return m_ptr->type() < other.m_ptr->type();

    return m_ptr->less(other.m_ptr.get());
}

/* * * * * * * * * * * * * * * * * * * *
 * Parsing
 */

/* esc(c)
 *
 * Format char c suitable for printing in an error message.
 */
static inline std::string esc(char c)
{
    char buf[12];
    if (static_cast<uint8_t>(c) >= 0x20 && static_cast<uint8_t>(c) <= 0x7f)
    {
        snprintf(buf, sizeof buf, "'%c' (%d)", c, c);
    }
    else
    {
        snprintf(buf, sizeof buf, "(%d)", c);
    }
    return std::string(buf);
}

static inline bool inRange(long x, long lower, long upper)
{
    return (x >= lower && x <= upper);
}

namespace
{
/* JsonParser
 *
 * Object that tracks all state of an in-progress parse.
 */
struct JsonParser final
{

    /* State
     */
    const std::string& str;
    size_t i;
    std::string& err;
    bool failed;

    /* fail(msg, err_ret = Json())
     *
     * Mark this parse as failed.
     */
    Json fail(std::string&& msg)
    {
        return fail(move(msg), Json());
    }

    template <typename T>
    T fail(std::string&& msg, const T errRet)
    {
        if (!failed)
            err = std::move(msg);
        failed = true;
        return errRet;
    }

    /* consumeWhitespace()
     *
     * Advance until the current character is non-whitespace.
     */
    void consumeWhitespace()
    {
        while (i < str.size()
            && (str[i] == ' ' || str[i] == '\r' || str[i] == '\n'
                || str[i] == '\t'))
        {
            i++;
        }
    }

    /* getNextToken()
     *
     * Return the next non-whitespace character. If the end of the input is
     * reached, flag an error and return 0.
     */
    char getNextToken()
    {
        consumeWhitespace();
        if (failed)
            return static_cast<char>(0);
        if (i == str.size())
            return fail("unexpected end of input", static_cast<char>(0));

        return str[i++];
    }

    /* encodeUtf8(pt, out)
     *
     * Encode pt as UTF-8 and add it to out.
     */
    void encodeUtf8(long pt, std::string& out)
    {
        if (pt < 0)
            return;

        if (pt < 0x80)
        {
            out += static_cast<char>(pt);
        }
        else if (pt < 0x800)
        {
            out += static_cast<char>((pt >> 6) | 0xC0);
            out += static_cast<char>((pt & 0x3F) | 0x80);
        }
        else if (pt < 0x10000)
        {
            out += static_cast<char>((pt >> 12) | 0xE0);
            out += static_cast<char>(((pt >> 6) & 0x3F) | 0x80);
            out += static_cast<char>((pt & 0x3F) | 0x80);
        }
        else
        {
            out += static_cast<char>((pt >> 18) | 0xF0);
            out += static_cast<char>(((pt >> 12) & 0x3F) | 0x80);
            out += static_cast<char>(((pt >> 6) & 0x3F) | 0x80);
            out += static_cast<char>((pt & 0x3F) | 0x80);
        }
    }

    /* parseString()
     *
     * Parse a std::string, starting at the current position.
     */
    std::string parseString()
    {
        std::string out;
        long lastEscapedCodepoint = -1;
        while (true)
        {
            if (i == str.size())
                return fail("unexpected end of input in std::string", "");

            char ch = str[i++];

            if (ch == '"')
            {
                encodeUtf8(lastEscapedCodepoint, out);
                return out;
            }

            if (inRange(ch, 0, 0x1f))
                return fail("unescaped " + esc(ch) + " in std::string", "");

            // The usual case: non-escaped characters
            if (ch != '\\')
            {
                encodeUtf8(lastEscapedCodepoint, out);
                lastEscapedCodepoint = -1;
                out += ch;
                continue;
            }

            // Handle escapes
            if (i == str.size())
                return fail("unexpected end of input in std::string", "");

            ch = str[i++];

            if (ch == 'u')
            {
                // Extract 4-byte escape sequence
                std::string esc = str.substr(i, 4);
                // Explicitly check length of the substring. The following loop
                // relies on std::string returning the terminating NUL when
                // accessing str[length]. Checking here reduces brittleness.
                if (esc.length() < 4)
                {
                    return fail("bad \\u escape: " + esc, "");
                }
                for (size_t j = 0; j < 4; j++)
                {
                    if (!inRange(esc[j], 'a', 'f') && !inRange(esc[j], 'A', 'F')
                        && !inRange(esc[j], '0', '9'))
                        return fail("bad \\u escape: " + esc, "");
                }

                long codepoint = strtol(esc.data(), nullptr, 16);

                // JSON specifies that characters outside the BMP shall be
                // encoded as a pair of 4-hex-digit \u escapes encoding their
                // surrogate pair components. Check whether we're in the middle
                // of such a beast: the previous codepoint was an escaped lead
                // (high) surrogate, and this is a trail (low) surrogate.
                if (inRange(lastEscapedCodepoint, 0xD800, 0xDBFF)
                    && inRange(codepoint, 0xDC00, 0xDFFF))
                {
                    // Reassemble the two surrogate pairs into one astral-plane
                    // character, per the UTF-16 algorithm.
                    encodeUtf8((((lastEscapedCodepoint - 0xD800) << 10)
                                   | (codepoint - 0xDC00))
                            + 0x10000,
                        out);
                    lastEscapedCodepoint = -1;
                }
                else
                {
                    encodeUtf8(lastEscapedCodepoint, out);
                    lastEscapedCodepoint = codepoint;
                }

                i += 4;
                continue;
            }

            encodeUtf8(lastEscapedCodepoint, out);
            lastEscapedCodepoint = -1;

            if (ch == 'b')
            {
                out += '\b';
            }
            else if (ch == 'f')
            {
                out += '\f';
            }
            else if (ch == 'n')
            {
                out += '\n';
            }
            else if (ch == 'r')
            {
                out += '\r';
            }
            else if (ch == 't')
            {
                out += '\t';
            }
            else if (ch == '"' || ch == '\\' || ch == '/')
            {
                out += ch;
            }
            else
            {
                return fail("invalid escape character " + esc(ch), "");
            }
        }
    }

    /* parseNumber()
     *
     * Parse a double.
     */
    Json parseNumber()
    {
        size_t startPos = i;
        bool isNegative = false;

        if (str[i] == '-')
        {
            i++;
            isNegative = true;
        }

        // Integer part
        if (str[i] == '0')
        {
            i++;
            if (inRange(str[i], '0', '9'))
                return fail("leading 0s not permitted in numbers");
        }
        else if (inRange(str[i], '1', '9'))
        {
            i++;
            while (inRange(str[i], '0', '9'))
                i++;
        }
        else
        {
            return fail("invalid " + esc(str[i]) + " in number");
        }

        if (str[i] != '.' && str[i] != 'e' && str[i] != 'E'
            && (i - startPos)
                // std::numeric_limits<T>::digits10 is guaranteed number of
                // digits that a number in type <T> without causing overflow or
                // loss of information. so the guaranteed value of digits<N>
                // will always suffer from "one less" discrepancy on platforms
                // where digits<N> is not a power of radix used for internal
                // representation. In non-exotic cases radix is 2. Since 10 is
                // not a power of 2, digits10 is smaller by 1 than the length of
                // the max value.
                <= (static_cast<size_t>(std::numeric_limits<int64_t>::digits10)
                    + (isNegative ? 2 : 1)))
        {
            return (std::int64_t)std::atoll(str.c_str() + startPos);
        }

        // Decimal part
        if (str[i] == '.')
        {
            i++;
            if (!inRange(str[i], '0', '9'))
                return fail("at least one digit required in fractional part");

            while (inRange(str[i], '0', '9'))
                i++;
        }

        // Exponent part
        if (str[i] == 'e' || str[i] == 'E')
        {
            i++;

            if (str[i] == '+' || str[i] == '-')
                i++;

            if (!inRange(str[i], '0', '9'))
                return fail("at least one digit required in exponent");

            while (inRange(str[i], '0', '9'))
                i++;
        }

        return std::strtod(str.c_str() + startPos, nullptr);
    }

    /* expect(str, res)
     *
     * Expect that 'str' starts at the character that was just read. If it does,
     * advance the input and return res. If not, flag an error.
     */
    Json expect(const std::string& expected, Json res)
    {
        assert(i != 0);
        i--;
        if (str.compare(i, expected.length(), expected) == 0)
        {
            i += expected.length();
            return res;
        }
        else
        {
            return fail("parse error: expected " + expected + ", got "
                + str.substr(i, expected.length()));
        }
    }

    /* parseJson()
     *
     * Parse a JSON Object.
     */
    Json parseJson(int depth)
    {
        if (depth > max_depth)
        {
            return fail("exceeded maximum nesting depth");
        }

        char ch = getNextToken();
        if (failed)
            return Json();

        if (ch == '-' || (ch >= '0' && ch <= '9'))
        {
            i--;
            return parseNumber();
        }

        if (ch == 't')
            return expect("true", true);

        if (ch == 'f')
            return expect("false", false);

        if (ch == 'n')
            return expect("null", Json());

        if (ch == '"')
            return parseString();

        if (ch == '{')
        {
            std::map<std::string, Json> data;
            ch = getNextToken();
            if (ch == '}')
                return data;

            while (1)
            {
                if (ch != '"')
                    return fail("expected '\"' in object, got " + esc(ch));

                std::string key = parseString();
                if (failed)
                    return Json();

                ch = getNextToken();
                if (ch != ':')
                    return fail("expected ':' in object, got " + esc(ch));

                data[std::move(key)] = parseJson(depth + 1);
                if (failed)
                    return Json();

                ch = getNextToken();
                if (ch == '}')
                    break;
                if (ch != ',')
                    return fail("expected ',' in object, got " + esc(ch));

                ch = getNextToken();
            }
            return data;
        }

        if (ch == '[')
        {
            std::vector<Json> data;
            ch = getNextToken();
            if (ch == ']')
                return data;

            while (1)
            {
                i--;
                data.push_back(parseJson(depth + 1));
                if (failed)
                    return Json();

                ch = getNextToken();
                if (ch == ']')
                    break;
                if (ch != ',')
                    return fail("expected ',' in list, got " + esc(ch));

                ch = getNextToken();
                (void)ch;
            }
            return data;
        }

        return fail("expected value, got " + esc(ch));
    }
};
}  // namespace

Json Json::parse(const std::string& in, std::string& err)
{
    JsonParser parser{in, 0, err, false};
    Json result = parser.parseJson(0);

    // Check for any trailing garbage
    parser.consumeWhitespace();
    if (parser.failed)
        return Json();
    if (parser.i != in.size())
        return parser.fail("unexpected trailing " + esc(in[parser.i]));

    return result;
}

std::vector<Json> Json::parseMulti(const std::string& in,
    std::string::size_type& parserStopPos,
    std::string& err)
{
    JsonParser parser{in, 0, err, false};
    parserStopPos = 0;
    std::vector<Json> jsonVec;
    while (parser.i != in.size() && !parser.failed)
    {
        jsonVec.push_back(parser.parseJson(0));
        if (parser.failed)
            break;

        // Check for another object
        parser.consumeWhitespace();
        if (parser.failed)
            break;
        parserStopPos = parser.i;
    }
    return jsonVec;
}

}  // namespace core

ROOT_NAMESPACE_END