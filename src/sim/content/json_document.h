#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

// content data の唯一の parse 入口。sim 内部専用の header なので
// `include/konbini/sim` へは出さない。
// @implements spec/data/content-schema.md Validation

namespace konbini::sim::json {

class Value {
public:
    struct Number {
        std::string lexeme;
        double value = 0.0;
    };

    using Object = std::map<std::string, Value, std::less<>>;
    using Array = std::vector<Value>;

    explicit Value(std::nullptr_t);
    explicit Value(bool value);
    explicit Value(Number value);
    explicit Value(std::string value);
    explicit Value(Array value);
    explicit Value(Object value);

    [[nodiscard]] bool isNumber() const noexcept;
    [[nodiscard]] const double& number() const;
    [[nodiscard]] std::string_view numberLexeme() const;
    [[nodiscard]] const std::string& string() const;
    [[nodiscard]] const Array& array() const;
    [[nodiscard]] const Object& object() const;

private:
    std::variant<std::nullptr_t, bool, Number, std::string, Array, Object> value_;
};

[[nodiscard]] Value parseDocument(std::string_view input);

}  // namespace konbini::sim::json
