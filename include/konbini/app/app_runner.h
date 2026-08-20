#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "konbini/app/app_paths.h"

// @implements spec/plan/tasks/first-playable.md Build and process boundary

namespace konbini::app {

// native app の起動設定。window と catch-up 上限だけを持ち、gameplay の
// balance 値は content 側 (`data/content/first-playable.json`) が正本。
struct AppRunnerConfig {
    AppPaths paths;
    std::uint32_t windowWidth = 1280;
    std::uint32_t windowHeight = 720;
    std::string windowTitle = "Konbini Dominant";
    std::uint32_t framesInFlight = 2;
    bool validation = false;
    // 1 フレームで消化する tick の上限。超過分は捨てて HUD へ出す。
    std::uint32_t maxTicksPerFrame = 5;
};

// 都市生成 -> simulation -> 入力 -> 描画を 1 本の frame loop へ束ねる
// composition root。
//
// 各責務 (content 読み込み / 都市 / simulation / 入力変換 / camera / 選択 /
// command 発行 / 描画 publish / frame graph) は専用の型が持ち、runner は
// 呼び出し順だけを決める。`GameManager` のように状態を集約しない。
class AppRunner {
public:
    explicit AppRunner(AppRunnerConfig config);
    ~AppRunner();

    AppRunner(const AppRunner&) = delete;
    AppRunner& operator=(const AppRunner&) = delete;

    // window が閉じられるまで回し、正常終了で 0 を返す。致命的な GPU 喪失は
    // 非 0 を返し、理由を stderr へ出す。
    [[nodiscard]] int run();

private:
    struct Impl;

    std::unique_ptr<Impl> impl_;
};

}  // namespace konbini::app
