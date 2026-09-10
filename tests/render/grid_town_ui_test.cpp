#include "../check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include "konbini/app/app_runner.h"
#include "konbini/app/pointer_controls.h"
#include "konbini/app/selection_controller.h"
#include "konbini/city/grid_town.h"
#include "konbini/city/city_manifest_projection.h"
#include "konbini/render/grid_ground.h"
#include "konbini/render/grid_store_connections.h"
#include "konbini/render/store_marker_geometry.h"
#include "konbini/render/vector_font.h"
#include "konbini/render/world_draw_list.h"
#include "konbini/render/zoc_overlay_geometry.h"
#include "konbini/sim/placement_system.h"

namespace {
using namespace konbini;
void gridPlacement() {
    sim::GenerationalIdPool<sim::FacilityId> ids;
    const auto town=city::makeGridTown(ids);
    CHECK(town.geometry.empty());
    auto facilities=city::projectFacilityTable(town.manifest);
    CHECK(facilities.size()==256);
    const auto content=sim::loadFirstPlayableContent(KONBINI_GRID_CONTENT_FILE);
    sim::GameState state;
    state.phase=sim::GamePhase::Phase1;
    state.playerChain=sim::ChainId::Losan;
    state.competitive=true;
    sim::ChainEconomyTable economy(content);
    CHECK(economy.activate(sim::ChainId::Losan));
    sim::StoreTable stores;
    sim::PopulationCellTable population;
    sim::GenerationalIdPool<sim::StoreId> storeIds;
    for(std::size_t i=0;i<facilities.size();++i) {
        const auto cell=facilities.row(i);
        CHECK(cell.isBuildable);
        CHECK(cell.boundsMeters.max.x-cell.boundsMeters.min.x==12);
        CHECK(cell.boundsMeters.max.z-cell.boundsMeters.min.z==12);
        const sim::PlaceStoreCommand command{{0,sim::CommandSourcePriority::Player,0},*state.playerChain,cell.id,0};
        CHECK(sim::validatePlacement(command,state,facilities,stores,economy).failure==sim::PlacementFailure::None);
    }
    for(const std::size_t index : {0U,15U,255U}) {
        const auto cell=facilities.row(index);
        const auto snapshot=sim::makeRenderSnapshot(state,content,facilities,stores,population,economy,{});
        const render::WorldRay ray{{cell.positionMeters.x,20,cell.positionMeters.z},{0,-1,0}};
        const auto picked=render::pickGridCell(ray,snapshot->facilities(),0);
        CHECK(picked && picked->facilityId==cell.id);
        app::SelectionController selection;
        const auto click=selection.onPrimaryClick(picked,*snapshot,true);
        CHECK(click.placementRequested && click.selected==cell.id);
        const auto before=economy.row(*state.playerChain).cashCredits;
        const sim::PlaceStoreCommand command{{0,sim::CommandSourcePriority::Player,0},*state.playerChain,cell.id,0};
        CHECK(sim::commitPlacementAtomically(command,state,facilities,stores,economy,storeIds).failure==sim::PlacementFailure::None);
        CHECK(economy.row(*state.playerChain).cashCredits==before-content.chain(*state.playerChain).buildCostCredits);
        CHECK(sim::validatePlacement(command,state,facilities,stores,economy).failure==sim::PlacementFailure::FacilityUnavailable ||
              sim::validatePlacement(command,state,facilities,stores,economy).failure==sim::PlacementFailure::FacilityOccupied);
    }
    const auto snapshot=sim::makeRenderSnapshot(state,content,facilities,stores,population,economy,{});
    CHECK(!render::pickGridCell({{1000,20,1000},{0,-1,0}},snapshot->facilities(),0));
    CHECK(!render::pickGridCell({{0,20,0},{1,0,0}},snapshot->facilities(),0));
    const auto first=snapshot->facilities().front();
    const auto next=render::pickGridCell({{first.boundsMeters.max.x,20,first.positionMeters.z},{0,-1,0}},
                                      snapshot->facilities(),0);
    CHECK(next && next->facilityId==snapshot->facilities()[1].id);
    auto spec=render::defaultWorldDrawListSpec();
    spec.gridTown=true;spec.storeMarker.gridCellMeters=12;spec.storeMarker.halfWidthMeters=6;
    const auto draw=render::buildWorldDrawList(*snapshot,{},spec);
    CHECK(draw.baseFacilities.empty() && draw.overlayFacilities.empty());
    CHECK(!draw.overlayMesh.indices.empty() && !draw.storeMesh.indices.empty());
}
sim::RenderStore store(std::uint32_t id,double x,double z) {
    sim::RenderStore result;
    result.id={{id,1}};result.facilityId={{id,1}};
    result.positionMeters={x,0,z};result.zocRadiusMeters=18;
    return result;
}
void connectedStores() {
    std::vector<sim::RenderStore> stores{store(0,6,6),store(1,18,6),store(2,6,18),store(3,30,30)};
    auto differentChain=store(4,30,6);differentChain.chain=sim::ChainId::Famoma;stores.push_back(differentChain);
    auto otherWorld=store(5,-6,6);otherWorld.dimension=1;stores.push_back(otherWorld);
    auto upper=store(6,-6,6);upper.verticalSlot=1;upper.positionMeters.y=3.2;stores.push_back(upper);
    auto anti=store(7,-6,6);anti.isAntiStore=true;stores.push_back(anti);
    const auto links=render::connectGridStores(stores,12);
    CHECK(links[0][render::Right]==1 && links[1][render::Left]==0);
    CHECK(links[0][render::Front]==2 && links[2][render::Back]==0);
    CHECK(!links[0][render::Left] && !links[1][render::Right]);
    for(const auto& edge:links[3]) CHECK(!edge); // diagonal only
    auto spec=render::defaultStoreMarkerSpec();
    spec.gridCellMeters=12;spec.halfWidthMeters=6;spec.heightMeters=2.8;
    const auto single=render::buildStoreMarkerGeometry(std::span(stores).first(1),spec);
    const auto pair=render::buildStoreMarkerGeometry(std::span(stores).first(2),spec);
    CHECK(pair.indices.size()<2*single.indices.size()); // one continuous sign
    const auto corner=render::buildStoreMarkerGeometry(std::span(stores).first(3),spec);
    for(const auto& vertex:corner.vertices) {
        CHECK(vertex.position[0]>=0 && vertex.position[0]<=24);
        CHECK(vertex.position[2]>=0 && vertex.position[2]<=24);
        CHECK(vertex.position[0]<=12 || vertex.position[2]<=12); // missing L-corner stays empty
    }
    auto elevated=store(8,6,6);elevated.positionMeters.y=320;
    const auto zoc=render::buildZocOverlayGeometry(std::span(&elevated,1),8,0.03F);
    CHECK(!zoc.vertices.empty());
    for(const auto& vertex:zoc.vertices) CHECK(std::abs(vertex.position[1]-320.03F)<0.001F);
}
double cross(const render::VectorFontPoint& a,const render::VectorFontPoint& b,double x,double y) {
    return (b[0]-a[0])*(y-a[1])-(b[1]-a[1])*(x-a[0]);
}
void vectorFontAndControls() {
    const auto glyph=render::vectorGlyph('O');
    CHECK(!glyph.indices.empty());
    int minX=32767,maxX=-32768,minY=32767,maxY=-32768;
    for(const auto p:glyph.points) {
        minX=std::min(minX,static_cast<int>(p[0]));maxX=std::max(maxX,static_cast<int>(p[0]));
        minY=std::min(minY,static_cast<int>(p[1]));maxY=std::max(maxY,static_cast<int>(p[1]));
    }
    const double x=(minX+maxX)*0.5,y=(minY+maxY)*0.5;
    bool fillsCounter=false;
    for(std::size_t i=0;i<glyph.indices.size();i+=3) {
        const auto a=glyph.points[glyph.indices[i]],b=glyph.points[glyph.indices[i+1]],c=glyph.points[glyph.indices[i+2]];
        CHECK(cross(a,b,c[0],c[1])>0);
        fillsCounter=fillsCounter || (cross(a,b,x,y)>=0 && cross(b,c,x,y)>=0 && cross(c,a,x,y)>=0);
    }
    CHECK(!fillsCounter);
    CHECK(render::vectorGlyph(' ').indices.empty());
    const app::AppRunnerConfig config;
    CHECK(config.windowWidth==1280 && config.windowHeight==720);
    app::HudTextInput input;
    input.gridPlacement=true;
    input.hud.phase=sim::GamePhase::Phase1;input.hud.playerChain=sim::ChainId::Losan;
    const auto controls=app::buildPointerControls(input,{1280,720},app::PointerPage::Main,1);
    CHECK(controls.buttons.size()==8);
    CHECK(std::find(controls.statusLines.begin(),controls.statusLines.end(),"TAP EMPTY GRID TO BUILD")!=controls.statusLines.end());
    const auto highDpi=app::buildPointerControls(input,{1280,720},app::PointerPage::Main,2);
    CHECK(highDpi.buttons.size()==8 && highDpi.uiScale==1);
    for(const auto& button:highDpi.buttons) CHECK(button.rect.y==highDpi.buttons.front().rect.y && button.rect.height==40);
    CHECK(controls.statusStyle.glyphPixelScale<=2);
    for(const auto& button:controls.buttons) {
        CHECK(button.rect.height<=40);
        CHECK(button.rect.y==controls.buttons.front().rect.y);
        CHECK(button.rect.x>=0 && button.rect.x+button.rect.width<=1280);
        CHECK(app::hitPointerControl(controls,button.rect.x+button.rect.width*0.5,
            button.rect.y+button.rect.height*0.5)==&button);
    }
    CHECK(!app::buildPointerControlMesh(controls,{1280,720},{}).indices.empty());
}
}
int main() {
    CHECK_NO_THROW(gridPlacement());
    CHECK_NO_THROW(connectedStores());
    CHECK_NO_THROW(vectorFontAndControls());
    return konbini::test::summarize("grid town and compact vector UI");
}
