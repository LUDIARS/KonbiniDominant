# data/ — データ契約

KonbiniDominant はDBを前提としないが、runtime table、content設定、saveデータを
明示的なschemaとして管理する。

- [world-state.md](world-state.md) — DoD runtime tables、command、event、tick境界
- [content-schema.md](content-schema.md) — chain / economy / phase / boss設定
- [save-format.md](save-format.md) — seed + recipe + gameplay delta の永続化

具体的なC++型名は実装で調整できるが、stable ID、所有境界、決定性の契約は変えない。
