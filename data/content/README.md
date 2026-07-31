# First-playable content

`first-playable.json` is the versioned owner of BASE-FP-CONTENT-01. Schema or
baseline changes must increment `contentVersion` and update canonical snapshot
expectations in the same change.

Content version `2` adds the required `residentPresentation` profile used only
to derive ambient resident render snapshots. It does not add residents to the
authoritative population/economy model or to save state.

The Phase 1 city integration is deliberately fixed to:

- Figmentum revision `3ee998f487d984f54003c4ec3c4f7ba00b53eec3`
- `fg::CityPlanParams{}` without consumer-side placement overrides
- world seed `42`
- per-facility marching-cubes resolution `24`
- `1 Figmentum unit = 1 meter`

Geometry is generated once during world loading. Empty, non-finite, or
out-of-range geometry is a load error; there is no placeholder-cube fallback.

`CityManifest` canonical serialization version `1` hashes fields with FNV-1a
64. It includes the upstream Figmentum facility key, canonical cell/lot,
recipe, bounds, and generator revision. Runtime generational `FacilityId`
values are deliberately excluded so a recreated world has the same save hash
without allowing stale runtime handles to alias.

Facility CPU geometry cache-key version `1` contains:

- exact Figmentum revision
- canonical building-recipe hash
- polygonize resolution (the current LOD input)
- vertex-format version

The current vertex format is position + deterministic accumulated triangle
normal + 32-bit index. Cache misses call Figmentum in process; cache hits return
an immutable geometry object.

World/runtime ID ownership stays outside the adapter:

```cpp
konbini::sim::WorldEntityIds ids;
konbini::adapters::figmentum::FigmentumCityAdapter cityGenerator;
auto generated = cityGenerator.generateFirstPlayableCity(ids.facilities());
auto facilities =
    konbini::city::projectFacilityTable(generated.manifest);
konbini::sim::FirstPlayableSimulation simulation(
    content, std::move(facilities), std::move(ids));
```
