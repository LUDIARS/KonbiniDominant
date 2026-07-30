#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

// @implements spec/interface/figmentum-city-generation.md Output: `CityManifest`

namespace konbini::city {

// 常に little-endian 固定幅で書き出す append-only writer。host endianness や
// struct padding に依存すると、同じ都市が環境ごとに別 hash になる。
// @implements spec/interface/figmentum-city-generation.md Output: `CityManifest`
class CanonicalBytes {
public:
    void u8(std::uint8_t value);
    void u32(std::uint32_t value);
    void u64(std::uint64_t value);
    void i32(std::int32_t value);
    void f64(double value);
    void text(std::string_view value);

    [[nodiscard]] const std::vector<std::byte>& bytes() const noexcept;
    [[nodiscard]] std::vector<std::byte> finish() &&;

private:
    std::vector<std::byte> bytes_;
};

// 非暗号学的 hash。生成物の同一性判定専用で、改竄検出には使わない。
[[nodiscard]] std::uint64_t fnv1a64(
    const std::vector<std::byte>& bytes) noexcept;

}  // namespace konbini::city
