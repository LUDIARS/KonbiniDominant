# Verification strategy

## 原則

- game resultを決めるcoreはheadless / deterministicに検証
- adapter fakeだけで実Vulkan / 実Figmentum経路の検証済みとしない
- random testはseedを出力し再現可能にする
- content値、recipe、save schemaをversionとともに固定
- 未実行testをgreenと報告しない

## 1. Data / ID unit tests

- generational IDのstale handle rejection
- all SoA streamsの同一length
- swap-pop後のsparse lookup
- structural commandをscan中に直接適用しない
- finite / range validation
- content missing keyのfail-fast

## 2. Deterministic simulation

同じworld seed、content hash、command streamで次を比較する。

- canonical state hash
- domain event sequence
- chain economy
- store / dimension / modifier tables
- Result

異なるrender frame cadence、container reserve容量、thread partitionでも一致させる。

## 3. Property tests

Phase 1:

- invalid placementでcash/world不変
- Triangleは3つのactive same-chain storeだけ
- degenerate triangle不成立
- vertex store破壊でtriangle即無効
- population / cashが負にならない

Phase 2:

- slot uniqueness
- vertical cost単調増加
- faith finite / clamp
- 256判定がslot数基準

Phase 3:

- same ordinal→same dimension
- explicit anchor以外で対消滅しない
- annihilationは各store1回
- collapse後にdimension参照なし

Boss:

- manifestation exactly 30 simulation seconds
- MaxValueにinteger overflowなし
- GameEnded後のcommand無効

## 4. Figmentum contract

- same recipe+seed→sameCityManifest
- central station anchor
- stable facility ID collisionなし
- manifest hash round-trip
- interactive facility recipe→non-empty mesh
- mesh boundsがmanifest bounds内
- generated normal finite / normalized
- cache keyにgenerator revision / LODを含む
- unsupported recipeをplaceholderへsilent fallbackしない

実Figmentum libraryをlinkしたcontract testを必須とし、fixture JSONだけで代替しない。

## 5. Pictor / Ergo integration

### Diagnostic gate

- Figmentum施設1棟を実GPU bufferへupload
- default passで表示
- pointer picking→FacilityId
- destroy event→Pictor unregister

### Production gate

- `GpuAssetStore` lifetime
- `IBatchGpuSource` resource resolution
- compiled render graph / submit / present
- resize / device cleanup
- static / dynamic / instance path
- facility置換 / store spawnの同frame差分
- dimension collapse時のbulk unregister
- GLFW callback→Ergo input→PlayerCommand

fake buffer / no-op presentではproduction gateを満たさない。

## 6. Save / migration

- save→load→canonical hash一致
- mesh/cache削除後も再生成
- JSON 64bit string round-trip
- content / manifest hash mismatchで明示失敗
- old schemaのforward migration
- migrationがoriginal saveを変更しない

## 7. AI

- candidate順がstable
- same seed/state→samecommand
- invalid cheat mutationなし
- action budget内
- playerと同じvalidationを通る

## 8. Performance

`TBD-PERF-01`でtargetを確定後、少なくとも次を計測する。

- max facilities / stores / population cells
- dimensions resident / simulated
- simulation tick p50 / p95 / max
- spatial query count
- Triangle rebuild cost
- draw calls / visible objects / triangles
- GPU / CPU mesh memory
- Figmentum generation / cache hit time
- 256階camera worst case

budget未設定のまま「高速」と判定しない。

## 9. Long run

- fixed seedのsoak
- AI込み全Phase→Boss
- repeated dimension create/collapse
- repeated save/load
- resource counterがbaselineへ戻る
- replay hash driftなし

## 10. Manual / UX

- placement reasonが読める
- ZOC / Triangleが色だけに依存しない
- vertical / dimension navigation
- boss 30秒warning
- motion / flash設定
- parody/IP review

manual確認はautomated rule testの代替にしない。
