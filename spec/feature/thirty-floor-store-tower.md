# Thirty-floor convenience-store tower

## Geometry

`buildStoreTowerGeometry` produces exactly 30 complete storefront floors.
DAYLARK, MOONPANTRY, SUNFOLD repeat from the bottom upward, giving ten floors
to each brand. Every floor keeps its sign, glazing, entry and canopy. A side
spine carries actual mesh floor numbers 01 through 30.

The nominal floor is 12 meters wide, 11.4 meters deep and 3.6 meters high;
the 30 floors total 108 meters. Individual storefront widths vary by up to
7.5 percent, with small repeatable horizontal offsets. Annexes, stairs,
the side spine and number plates extend beyond the nominal footprint.
There is no extra decorative thirty-first store.

## Dense additions

The 2026-09-07 follow-up asks for a Kowloon-inspired appearance. Two service
annexes reach 27 and 24 floors, creating unequal rooflines around the thirty
branded stores. Their projecting slabs, patched metal awnings, switchback
stairs, air conditioners, drain stacks and rooftop tanks give the silhouette
the appearance of incremental additions. Muted plaster alternates with lit
window panes; vertical OPEN / 24H / MART / FOOD signs add accents among the
facades. These signs use opaque colored geometry, not emissive lighting.

All variation is deterministic. Annex geometry is isolated in
`store_tower_additions`; the tower composer retains ownership of the thirty
storefronts and floor numbers. Annexes add no simulated stores or playable
rooms. This is an architectural motif, not a reconstruction of Kowloon.

This is a game-owned presentation mesh. No tower placement rule, economy,
collision, elevators, or playable upper-floor navigation is added by this asset.

## Design decision from comparison

Current main `4678511` is the Fable version, as identified by neco. Its stores
are 6 x 6 x 7 meter translucent boxes, 24 vertices / 12 triangles each. The
new storefronts use more geometry but communicate convenience-store identity
through lettering, entrances and awnings. The tower uses the new storefronts
so the repeated floors read as stacked shops instead of a generic skyscraper.
GPU performance was not benchmarked; no speedup claim is made.

## Delivery

Pictor captures include the full 30-floor silhouette and a close-up of the
upper numbered floors. The same deterministic geometry is available through
the render-domain API for later gameplay integration.

[Full tower, 900 x 1600](store-gallery/TOWER-30.png) /
[Upper-floor detail, 1600 x 900](store-gallery/TOWER-DETAIL.png)

The Release gallery build succeeded with tests disabled. Both images were
exported from Pictor through the main-folder Excubitor service, losslessly
encoded as PNG, and visually inspected. The full image shows floors 01-30;
the updated detail includes the upper annex terraces and three distinct
facades. The denser version was rebuilt and captured through the same service.
No unit/integration
tests or full-game validation were run.
