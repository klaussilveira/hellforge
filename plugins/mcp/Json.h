#pragma once

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <sstream>
#include <stdexcept>
#include <cmath>

namespace mcp
{

class JsonValue
{
public:
    using Object = std::map<std::string, JsonValue>;
    using Array = std::vector<JsonValue>;
    using Value = std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;

    JsonValue() : _value(nullptr) {}
    JsonValue(std::nullptr_t) : _value(nullptr) {}
    JsonValue(bool b) : _value(b) {}
    JsonValue(int i) : _value(static_cast<double>(i)) {}
    JsonValue(std::size_t i) : _value(static_cast<double>(i)) {}
    JsonValue(double d) : _value(d) {}
    JsonValue(const char* s) : _value(std::string(s)) {}
    JsonValue(const std::string& s) : _value(s) {}
    JsonValue(std::string&& s) : _value(std::move(s)) {}
    JsonValue(const Array& a) : _value(a) {}
    JsonValue(Array&& a) : _value(std::move(a)) {}
    JsonValue(const Object& o) : _value(o) {}
    JsonValue(Object&& o) : _value(std::move(o)) {}

    bool isNull() const { return std::holds_alternative<std::nullptr_t>(_value); }
    bool isBool() const { return std::holds_alternative<bool>(_value); }
    bool isNumber() const { return std::holds_alternative<double>(_value); }
    bool isString() const { return std::holds_alternative<std::string>(_value); }
    bool isArray() const { return std::holds_alternative<Array>(_value); }
    bool isObject() const { return std::holds_alternative<Object>(_value); }

    bool getBool() const { return std::get<bool>(_value); }
    double getNumber() const { return std::get<double>(_value); }
    int getInt() const { return static_cast<int>(std::get<double>(_value)); }
    const std::string& getString() const { return std::get<std::string>(_value); }
    const Array& getArray() const { return std::get<Array>(_value); }
    Array& getArray() { return std::get<Array>(_value); }
    const Object& getObject() const { return std::get<Object>(_value); }
    Object& getObject() { return std::get<Object>(_value); }

    // Object member access
    const JsonValue& operator[](const std::string& key) const
    {
        static JsonValue null;
        auto& obj = std::get<Object>(_value);
        auto it = obj.find(key);
        return it != obj.end() ? it->second : null;
    }

    bool has(const std::string& key) const
    {
        if (!isObject()) return false;
        return std::get<Object>(_value).count(key) > 0;
    }

    // Array element access
    const JsonValue& operator[](std::size_t index) const
    {
        return std::get<Array>(_value).at(index);
    }

    std::size_t size() const
    {
        if (isArray()) return std::get<Array>(_value).size();
        if (isObject()) return std::get<Object>(_value).size();
        return 0;
    }

    // Serialize to JSON string
    std::string dump() const
    {
        std::ostringstream ss;
        serialize(ss);
        return ss.str();
    }

    void serialize(std::ostream& os) const
    {
        if (isNull()) { os << "null"; }
        else if (isBool()) { os << (getBool() ? "true" : "false"); }
        else if (isNumber())
        {
            double d = getNumber();
            if (d == std::floor(d) && std::abs(d) < 1e15)
                os << static_cast<long long>(d);
            else
                os << d;
        }
        else if (isString()) { os << '"'; escapeString(os, getString()); os << '"'; }
        else if (isArray())
        {
            os << '[';
            bool first = true;
            for (auto& v : getArray())
            {
                if (!first) os << ',';
                v.serialize(os);
                first = false;
            }
            os << ']';
        }
        else if (isObject())
        {
            os << '{';
            bool first = true;
            for (auto& [k, v] : getObject())
            {
                if (!first) os << ',';
                os << '"'; escapeString(os, k); os << "\":";
                v.serialize(os);
                first = false;
            }
            os << '}';
        }
    }

    // Parse JSON from string
    static JsonValue parse(const std::string& input)
    {
        std::size_t pos = 0;
        auto result = parseValue(input, pos);
        return result;
    }

private:
    Value _value;

    static void escapeString(std::ostream& os, const std::string& s)
    {
        for (char c : s)
        {
            switch (c)
            {
                case '"': os << "\\\""; break;
                case '\\': os << "\\\\"; break;
                case '\n': os << "\\n"; break;
                case '\r': os << "\\r"; break;
                case '\t': os << "\\t"; break;
                default: os << c; break;
            }
        }
    }

    static void skipWhitespace(const std::string& s, std::size_t& pos)
    {
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r'))
            ++pos;
    }

    static JsonValue parseValue(const std::string& s, std::size_t& pos)
    {
        skipWhitespace(s, pos);
        if (pos >= s.size()) throw std::runtime_error("Unexpected end of JSON");

        char c = s[pos];
        if (c == '"') return parseString(s, pos);
        if (c == '{') return parseObject(s, pos);
        if (c == '[') return parseArray(s, pos);
        if (c == 't' || c == 'f') return parseBool(s, pos);
        if (c == 'n') return parseNull(s, pos);
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber(s, pos);
        throw std::runtime_error(std::string("Unexpected character: ") + c);
    }

    static JsonValue parseString(const std::string& s, std::size_t& pos)
    {
        ++pos; // skip opening quote
        std::string result;
        while (pos < s.size() && s[pos] != '"')
        {
            if (s[pos] == '\\')
            {
                ++pos;
                if (pos >= s.size()) throw std::runtime_error("Unterminated string escape");
                switch (s[pos])
                {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    default: result += s[pos]; break;
                }
            }
            else
            {
                result += s[pos];
            }
            ++pos;
        }
        if (pos >= s.size()) throw std::runtime_error("Unterminated string");
        ++pos; // skip closing quote
        return JsonValue(std::move(result));
    }

    static JsonValue parseNumber(const std::string& s, std::size_t& pos)
    {
        std::size_t start = pos;
        if (s[pos] == '-') ++pos;
        while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') ++pos;
        if (pos < s.size() && s[pos] == '.')
        {
            ++pos;
            while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') ++pos;
        }
        if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E'))
        {
            ++pos;
            if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) ++pos;
            while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') ++pos;
        }
        return JsonValue(std::stod(s.substr(start, pos - start)));
    }

    static JsonValue parseBool(const std::string& s, std::size_t& pos)
    {
        if (s.compare(pos, 4, "true") == 0) { pos += 4; return JsonValue(true); }
        if (s.compare(pos, 5, "false") == 0) { pos += 5; return JsonValue(false); }
        throw std::runtime_error("Invalid boolean");
    }

    static JsonValue parseNull(const std::string& s, std::size_t& pos)
    {
        if (s.compare(pos, 4, "null") == 0) { pos += 4; return JsonValue(nullptr); }
        throw std::runtime_error("Invalid null");
    }

    static JsonValue parseArray(const std::string& s, std::size_t& pos)
    {
        ++pos; // skip '['
        Array arr;
        skipWhitespace(s, pos);
        if (pos < s.size() && s[pos] == ']') { ++pos; return JsonValue(std::move(arr)); }
        while (true)
        {
            arr.push_back(parseValue(s, pos));
            skipWhitespace(s, pos);
            if (pos < s.size() && s[pos] == ',') { ++pos; continue; }
            if (pos < s.size() && s[pos] == ']') { ++pos; break; }
            throw std::runtime_error("Expected ',' or ']' in array");
        }
        return JsonValue(std::move(arr));
    }

    static JsonValue parseObject(const std::string& s, std::size_t& pos)
    {
        ++pos; // skip '{'
        Object obj;
        skipWhitespace(s, pos);
        if (pos < s.size() && s[pos] == '}') { ++pos; return JsonValue(std::move(obj)); }
        while (true)
        {
            skipWhitespace(s, pos);
            if (pos >= s.size() || s[pos] != '"') throw std::runtime_error("Expected string key in object");
            auto key = parseString(s, pos);
            skipWhitespace(s, pos);
            if (pos >= s.size() || s[pos] != ':') throw std::runtime_error("Expected ':' in object");
            ++pos;
            obj[key.getString()] = parseValue(s, pos);
            skipWhitespace(s, pos);
            if (pos < s.size() && s[pos] == ',') { ++pos; continue; }
            if (pos < s.size() && s[pos] == '}') { ++pos; break; }
            throw std::runtime_error("Expected ',' or '}' in object");
        }
        return JsonValue(std::move(obj));
    }
};

// Helper to build JSON objects
inline JsonValue jsonObject(std::initializer_list<std::pair<std::string, JsonValue>> pairs)
{
    JsonValue::Object obj;
    for (auto& p : pairs)
        obj[p.first] = p.second;
    return JsonValue(std::move(obj));
}

inline JsonValue jsonArray(std::initializer_list<JsonValue> values)
{
    return JsonValue(JsonValue::Array(values));
}

// JSON-RPC helpers
inline JsonValue jsonRpcResult(const JsonValue& id, const JsonValue& result)
{
    return jsonObject({{"jsonrpc", "2.0"}, {"id", id}, {"result", result}});
}

inline JsonValue jsonRpcError(const JsonValue& id, int code, const std::string& message)
{
    return jsonObject({
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", jsonObject({{"code", code}, {"message", message}})}
    });
}

} // namespace mcp
