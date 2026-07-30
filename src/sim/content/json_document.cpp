#include "json_document.h"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <system_error>
#include <utility>

// @implements spec/data/content-schema.md Validation

namespace konbini::sim::json {

namespace {

class Parser {
public:
    explicit Parser(std::string_view input);

    [[nodiscard]] Value parse() {
        skipWhitespace();
        Value result = parseValue(0);
        skipWhitespace();
        if (position_ != input_.size()) {
            fail("unexpected trailing data");
        }
        return result;
    }

private:
    static constexpr std::size_t kMaximumDepth = 32;

    [[nodiscard]] Value parseValue(const std::size_t depth) {
        if (depth > kMaximumDepth) {
            fail("maximum JSON nesting depth exceeded");
        }
        if (position_ >= input_.size()) {
            fail("unexpected end of JSON input");
        }

        switch (input_[position_]) {
            case '{':
                return Value(parseObject(depth + 1));
            case '[':
                return Value(parseArray(depth + 1));
            case '"':
                return Value(parseString());
            case 't':
                consumeLiteral("true");
                return Value(true);
            case 'f':
                consumeLiteral("false");
                return Value(false);
            case 'n':
                consumeLiteral("null");
                return Value(nullptr);
            default:
                return Value(parseNumber());
        }
    }

    [[nodiscard]] Value::Object parseObject(const std::size_t depth) {
        expect('{');
        skipWhitespace();
        Value::Object object;
        if (consumeIf('}')) {
            return object;
        }

        while (true) {
            if (position_ >= input_.size() || input_[position_] != '"') {
                fail("object key must be a string");
            }
            std::string key = parseString();
            skipWhitespace();
            expect(':');
            skipWhitespace();
            Value value = parseValue(depth);
            if (!object.emplace(std::move(key), std::move(value)).second) {
                fail("duplicate object key");
            }
            skipWhitespace();
            if (consumeIf('}')) {
                return object;
            }
            expect(',');
            skipWhitespace();
        }
    }

    [[nodiscard]] Value::Array parseArray(const std::size_t depth) {
        expect('[');
        skipWhitespace();
        Value::Array array;
        if (consumeIf(']')) {
            return array;
        }

        while (true) {
            array.push_back(parseValue(depth));
            skipWhitespace();
            if (consumeIf(']')) {
                return array;
            }
            expect(',');
            skipWhitespace();
        }
    }

    [[nodiscard]] std::string parseString() {
        expect('"');
        std::string result;
        while (position_ < input_.size()) {
            const unsigned char value = static_cast<unsigned char>(input_[position_++]);
            if (value == '"') {
                return result;
            }
            if (value == '\\') {
                parseEscape(result);
                continue;
            }
            if (value < 0x20U) {
                fail("unescaped control character in string");
            }
            result.push_back(static_cast<char>(value));
        }
        fail("unterminated string");
    }

    void parseEscape(std::string& result) {
        if (position_ >= input_.size()) {
            fail("unterminated string escape");
        }
        const char escaped = input_[position_++];
        switch (escaped) {
            case '"':
            case '\\':
            case '/':
                result.push_back(escaped);
                return;
            case 'b':
                result.push_back('\b');
                return;
            case 'f':
                result.push_back('\f');
                return;
            case 'n':
                result.push_back('\n');
                return;
            case 'r':
                result.push_back('\r');
                return;
            case 't':
                result.push_back('\t');
                return;
            case 'u':
                appendCodePoint(result, parseUnicodeEscape());
                return;
            default:
                fail("unsupported string escape");
        }
    }

    [[nodiscard]] std::uint32_t parseUnicodeEscape() {
        if (position_ + 4 > input_.size()) {
            fail("truncated unicode escape");
        }
        std::uint32_t value = 0;
        for (int index = 0; index < 4; ++index) {
            value <<= 4U;
            const char digit = input_[position_++];
            if (digit >= '0' && digit <= '9') {
                value += static_cast<std::uint32_t>(digit - '0');
            } else if (digit >= 'a' && digit <= 'f') {
                value += static_cast<std::uint32_t>(digit - 'a' + 10);
            } else if (digit >= 'A' && digit <= 'F') {
                value += static_cast<std::uint32_t>(digit - 'A' + 10);
            } else {
                fail("invalid unicode escape");
            }
        }
        if (value >= 0xD800U && value <= 0xDFFFU) {
            fail("surrogate unicode escapes are not supported");
        }
        return value;
    }

    static void appendCodePoint(std::string& output, const std::uint32_t codePoint) {
        if (codePoint <= 0x7FU) {
            output.push_back(static_cast<char>(codePoint));
        } else if (codePoint <= 0x7FFU) {
            output.push_back(static_cast<char>(0xC0U | (codePoint >> 6U)));
            output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
        } else {
            output.push_back(static_cast<char>(0xE0U | (codePoint >> 12U)));
            output.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
        }
    }

    [[nodiscard]] Value::Number parseNumber() {
        const std::size_t start = position_;
        if (consumeIf('-') && position_ >= input_.size()) {
            fail("truncated number");
        }
        if (consumeIf('0')) {
            if (position_ < input_.size() && input_[position_] >= '0' &&
                input_[position_] <= '9') {
                fail("number has a leading zero");
            }
        } else {
            consumeDigits(true);
        }
        if (consumeIf('.')) {
            consumeDigits(true);
        }
        if (position_ < input_.size() &&
            (input_[position_] == 'e' || input_[position_] == 'E')) {
            ++position_;
            if (position_ < input_.size() &&
                (input_[position_] == '+' || input_[position_] == '-')) {
                ++position_;
            }
            consumeDigits(true);
        }

        double value = 0.0;
        const char* begin = input_.data() + start;
        const char* end = input_.data() + position_;
        const auto parsed = std::from_chars(begin, end, value);
        if (parsed.ec != std::errc{} || parsed.ptr != end || !std::isfinite(value)) {
            fail("invalid or non-finite number");
        }
        return {
            .lexeme = std::string(begin, end),
            .value = value,
        };
    }

    void consumeDigits(const bool requireOne) {
        const std::size_t start = position_;
        while (position_ < input_.size() && input_[position_] >= '0' &&
               input_[position_] <= '9') {
            ++position_;
        }
        if (requireOne && start == position_) {
            fail("expected decimal digit");
        }
    }

    void consumeLiteral(const std::string_view literal) {
        if (input_.substr(position_, literal.size()) != literal) {
            fail("invalid JSON token");
        }
        position_ += literal.size();
    }

    void expect(const char expected) {
        if (position_ >= input_.size() || input_[position_] != expected) {
            fail("unexpected JSON token");
        }
        ++position_;
    }

    [[nodiscard]] bool consumeIf(const char expected) noexcept {
        if (position_ < input_.size() && input_[position_] == expected) {
            ++position_;
            return true;
        }
        return false;
    }

    void skipWhitespace() noexcept {
        while (position_ < input_.size()) {
            const char value = input_[position_];
            if (value != ' ' && value != '\t' && value != '\r' && value != '\n') {
                return;
            }
            ++position_;
        }
    }

    [[noreturn]] void fail(const std::string_view message) const {
        throw std::invalid_argument(std::string(message) + " at byte " +
                                    std::to_string(position_));
    }

    std::string_view input_;
    std::size_t position_ = 0;
};

Parser::Parser(const std::string_view input) : input_(input) {}

template <typename T>
const T& require(const std::variant<std::nullptr_t,
                                    bool,
                                    Value::Number,
                                    std::string,
                                    Value::Array,
                                    Value::Object>& value,
                 const char* expected) {
    const T* result = std::get_if<T>(&value);
    if (result == nullptr) {
        throw std::invalid_argument(std::string("JSON value is not ") + expected);
    }
    return *result;
}

}  // namespace

// 型は construct 時に確定し、以後は accessor が要求型と一致しなければ throw
// する。未検証の JSON 値を既定値へ黙って落とさない。
// @implements spec/data/content-schema.md Validation
Value::Value(std::nullptr_t) : value_(nullptr) {}
Value::Value(const bool value) : value_(value) {}
Value::Value(Number value) : value_(std::move(value)) {}
Value::Value(std::string value) : value_(std::move(value)) {}
Value::Value(Array value) : value_(std::move(value)) {}
Value::Value(Object value) : value_(std::move(value)) {}

bool Value::isNumber() const noexcept {
    return std::holds_alternative<Number>(value_);
}

const double& Value::number() const {
    return require<Number>(value_, "a number").value;
}

std::string_view Value::numberLexeme() const {
    return require<Number>(value_, "a number").lexeme;
}

const std::string& Value::string() const {
    return require<std::string>(value_, "a string");
}

const Value::Array& Value::array() const {
    return require<Array>(value_, "an array");
}

const Value::Object& Value::object() const {
    return require<Object>(value_, "an object");
}

// @implements spec/data/content-schema.md Validation
Value parseDocument(const std::string_view input) {
    if (input.empty()) {
        throw std::invalid_argument("JSON document is empty");
    }
    if (input.size() >= 3 &&
        static_cast<unsigned char>(input[0]) == 0xEFU &&
        static_cast<unsigned char>(input[1]) == 0xBBU &&
        static_cast<unsigned char>(input[2]) == 0xBFU) {
        throw std::invalid_argument("JSON document must be UTF-8 without BOM");
    }
    return Parser(input).parse();
}

}  // namespace konbini::sim::json
