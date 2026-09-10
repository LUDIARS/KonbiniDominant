#include "../check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include "konbini/adapters/ergo/construction_presentation.h"
#include "konbini/render/animated_store_geometry.h"
// @implements spec/feature/store-construction-effects.md Playback lifecycle
namespace {
using namespace konbini;
struct Fixture {
    sim::FirstPlayableContent content=sim::loadFirstPlayableContent(KONBINI_CONSTRUCTION_CONTENT_FILE);
    sim::GameState state;
    sim::FacilityTable facilities;
    sim::StoreTable stores;
    sim::PopulationCellTable population;
    sim::ChainEconomyTable economy{content};
    Fixture() {
        state.phase=sim::GamePhase::Phase1;state.competitive=true;state.playerChain=sim::ChainId::Losan;
        CHECK(economy.activate(sim::ChainId::Losan));
        for(std::uint32_t i=0;i<2;++i) {
            sim::FacilityRow f;
            f.id={{i,1}};f.figmentumKey={i+1};f.positionMeters={i*12.0,0,0};
            f.boundsMeters={{i*12.0-6,0,-6},{i*12.0+6,1,6}};f.isBuildable=true;
            facilities.append(f);
            sim::StoreRow s;
            s.id={{i,1}};s.facilityId=f.id;s.positionMeters=f.positionMeters;s.zocRadiusMeters=24;
            stores.append(s);
        }
    }
    std::shared_ptr<const sim::RenderSnapshot> snapshot(std::uint64_t tick,std::initializer_list<unsigned> ids) {
        state.completedTicks=tick;
        std::vector<sim::RenderStorePlacementCue> cues;
        for(auto id:ids) cues.push_back({{{id,1}},stores.row(id).positionMeters,0,tick});
        return sim::makeRenderSnapshot(state,content,facilities,stores,population,economy,cues,{},{});
    }
};
void playbackDoesNotLoseIntermediateTicks() {
    Fixture f;adapters::ergo::ConstructionPresentation playback;
    auto first=f.snapshot(1,{0});playback.observe(*first);playback.observe(*first);
    CHECK(playback.visuals().size()==1);
    auto second=f.snapshot(2,{1});playback.observe(*second);
    CHECK(playback.visuals().size()==2);
    const auto camera=render::buildIsometricCamera({}, {1280,720});
    CHECK(!playback.particleMesh(camera,second->stores()).vertices.empty());
    CHECK(playback.particleMesh(camera,{}).vertices.empty());
    for(int i=0;i<9;++i)playback.advance(0.1);
    CHECK(playback.visuals().size()==2 && playback.visuals()[0].animation.hasLanded);
    const auto dust=playback.particleMesh(camera,second->stores());
    CHECK(!dust.vertices.empty());
    CHECK(std::ranges::any_of(dust.vertices,[](const auto& v){return v.color[0]>v.color[2] && v.color[3]>0;}));
    for(const auto& v:dust.vertices)for(float x:v.position)CHECK(std::isfinite(x));
    for(auto index:dust.indices)CHECK(index<dust.vertices.size());
    for(int i=0;i<8;++i)playback.advance(0.1);
    CHECK(playback.visuals().empty());
    CHECK(playback.particleMesh(camera,second->stores()).vertices.empty());
    playback.observe(*first);
    CHECK(playback.visuals().size()==1);
    CHECK(f.stores.deactivate({{0,1}}));
    playback.observe(*f.snapshot(3,{}));
    CHECK(playback.visuals().empty());
    playback.reset();
    CHECK(playback.visuals().empty());
}
void cellsConnectOnlyAfterImpact() {
    Fixture f;const auto snapshot=f.snapshot(1,{1});
    auto spec=render::defaultStoreMarkerSpec();spec.gridCellMeters=12;spec.heightMeters=3;
    render::StoreConstructionVisual visual{snapshot->stores()[1],
        render::sampleStorePlacementAnimation(render::defaultStorePlacementAnimationSpec(),
            {snapshot->stores()[1].positionMeters,0},0.2),0.2};
    const auto airborne=render::buildAnimatedStoreGeometry(snapshot->stores(),spec,{&visual,1});
    CHECK(std::ranges::any_of(airborne.vertices,[](const auto& v){return v.position[1]>8;}));
    visual.animation=render::sampleStorePlacementAnimation(render::defaultStorePlacementAnimationSpec(),
        {visual.store.positionMeters,0},1);
    const auto landed=render::buildAnimatedStoreGeometry(snapshot->stores(),spec,{&visual,1});
    const auto settled=render::buildStoreMarkerGeometry(snapshot->stores(),spec);
    CHECK(landed.indices==settled.indices && landed.vertices.size()==settled.vertices.size());
    for(std::size_t i=0;i<landed.vertices.size();++i)CHECK(landed.vertices[i].position==settled.vertices[i].position);
}
}
int main() {
    CHECK_NO_THROW(playbackDoesNotLoseIntermediateTicks());
    CHECK_NO_THROW(cellsConnectOnlyAfterImpact());
    return konbini::test::summarize("Ergo construction presentation");
}
