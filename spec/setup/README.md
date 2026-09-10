# setup/ — 開発環境

- [native-development.md](native-development.md) — C++ / CMake / Pictor / Ergo /
  Figmentum の native 構成
- [phase1-portable-build.md](phase1-portable-build.md) — Windows portable build
- [mobile-development.md](mobile-development.md) — mobile toolchain / platform 境界の要求
- [mobile-native.md](mobile-native.md) — Android / iOS host の実装 baseline と未検証項目

Native の CMake target と Android / iOS host は実装済み。mobile build、install、
device launch、touch、lifecycle は未検証であり、`mobile-native.md` の確認項目を
満たすまでは対応済みと扱わない。
