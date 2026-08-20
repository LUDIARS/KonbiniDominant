#pragma once

#include <cstdint>

// @implements spec/interface/ergo-runtime.md Frame / simulation
// @implements spec/design.md 5. Tick と決定性

namespace konbini::app {

// 1 フレーム分の tick 計画。`droppedTicks` が 0 でないとき、accumulator から
// 捨てた分がある (catch-up 上限)。silent に落とすと「重いのに時間が進んで
// いる」状態を観測できなくなるので、host 側が log する前提で返す。
struct FixedStepPlan {
    std::uint32_t tickCount = 0;
    std::uint32_t droppedTicks = 0;
};

// render frame の dt を simulation の fixed tick へ変換する accumulator。
//
// render dt を simulation へ直接積むと frame rate が canonical state を変える
// ため、tick 幅は content の `ticksPerSecond` で固定する。1 フレームで消化
// できる tick 数には上限を設け、超過分は捨てて報告する (spiral of death を
// 起こさない)。
class FixedStepDriver {
public:
    // `maxTicksPerFrame` は catch-up 上限。0 は不正 (tick が一切進まない)。
    FixedStepDriver(
        std::uint32_t ticksPerSecond, std::uint32_t maxTicksPerFrame);

    [[nodiscard]] FixedStepPlan advance(double renderDtSeconds);

    [[nodiscard]] double fixedDeltaSeconds() const noexcept;
    [[nodiscard]] double accumulatedSeconds() const noexcept;
    [[nodiscard]] std::uint32_t maxTicksPerFrame() const noexcept;

    // pause / window 復帰などで溜まった時間を捨てる。捨てた事実は
    // 呼び出し側が扱う (戻り値の秒数)。
    double drain() noexcept;

private:
    double fixedDeltaSeconds_ = 0.0;
    double accumulatedSeconds_ = 0.0;
    std::uint32_t maxTicksPerFrame_ = 0;
};

// 1 フレームの dt をこの値で clamp する。debugger で止めた後や minimize から
// 復帰した直後に巨大な dt が入ると、catch-up 上限だけでは大量の tick を
// 落とし続けることになる。
inline constexpr double kMaxRenderDeltaSeconds = 0.25;

}  // namespace konbini::app
