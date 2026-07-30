#include "canonical_bytes.h"

#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

// @implements spec/interface/figmentum-city-generation.md Output: `CityManifest`

namespace konbini::city {

void CanonicalBytes::u8(const std::uint8_t value) {
    bytes_.push_back(static_cast<std::byte>(value));
}

void CanonicalBytes::u32(const std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
        u8(static_cast<std::uint8_t>(value >> shift));
    }
}

void CanonicalBytes::u64(const std::uint64_t value) {
    for (unsigned shift = 0; shift < 64; shift += 8) {
        u8(static_cast<std::uint8_t>(value >> shift));
    }
}

void CanonicalBytes::i32(const std::int32_t value) {
    u32(std::bit_cast<std::uint32_t>(value));
}

// NaN は bit pattern が一意でなく、比較も自己不一致になるため canonical
// 表現を持てない。infinity も同様に plan の不具合として弾く。
void CanonicalBytes::f64(const double value) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument(
            "canonical city data cannot contain non-finite numbers");
    }
    u64(std::bit_cast<std::uint64_t>(value));
}

// length prefix + raw bytes。区切り文字を使うと、文字列自身が区切りを含む
// ときに別の入力が同じ byte 列へ潰れる。
void CanonicalBytes::text(const std::string_view value) {
    if (value.size() >
        static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
        throw std::overflow_error("canonical city string is too large");
    }
    u32(static_cast<std::uint32_t>(value.size()));
    for (const char byte : value) {
        u8(static_cast<std::uint8_t>(byte));
    }
}

const std::vector<std::byte>& CanonicalBytes::bytes() const noexcept {
    return bytes_;
}

std::vector<std::byte> CanonicalBytes::finish() && {
    return std::move(bytes_);
}

std::uint64_t fnv1a64(const std::vector<std::byte>& bytes) noexcept {
    std::uint64_t hash = 14695981039346656037ull;
    for (const std::byte value : bytes) {
        hash ^= static_cast<std::uint8_t>(value);
        hash *= 1099511628211ull;
    }
    return hash;
}

}  // namespace konbini::city
