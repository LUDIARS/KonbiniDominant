// @implements spec/feature/pointer-controls.md
#include "konbini/app/pointer_controls.h"
#include "konbini/app/skill_text.h"
#include "konbini/app/pointer_hud_text.h"
#include <algorithm>
#include <cmath>
namespace konbini::app {
PointerControls buildPointerControls(const HudTextInput& input,const render::ViewportExtent extent,
                                     const PointerPage page,const double uiScale) {
    PointerControls result;
    result.uiScale=static_cast<float>(std::min({std::clamp(uiScale,0.75,1.25),
        extent.width/320.0,extent.height/720.0}));
    const auto& h=input.hud;
    const bool choosing=h.phase==sim::GamePhase::ChainSelect;
    const bool skills=h.campaign.enabled && h.campaign.skills.pending;
    const bool ended=h.phase==sim::GamePhase::Result;
    result.modal=choosing || skills || ended;
    const auto add=[&](PointerAction action,std::string label,bool enabled=true,std::string detail="",bool repeat=false) {
        result.buttons.push_back({action,{},std::move(label),std::move(detail),enabled,repeat});
    };
    if(choosing) {
        result.heading="CHOOSE YOUR CONVENIENCE STORE";
        result.modalLines={"ONE CHAIN FOR THE ENTIRE RUN","TAP AN EMPTY GRID CELL TO BUILD / DRAG TO MOVE"};
        for(unsigned i=0;i<3;++i)
            add(static_cast<PointerAction>(i),hudChainLabel(static_cast<sim::ChainId>(i)),true,
                "BUILD COST "+std::to_string(h.chains[i].buildCost));
    } else if(skills) {
        result.heading="LEVEL UP - TAP ONE UPGRADE";
        for(unsigned i=0;i<3;++i) {
            const auto id=h.campaign.skills.offers[i];
            if(id==sim::SkillId::Count) continue;
            const auto rank=sim::skillRank(h.campaign.skills,id)+1;
            add(static_cast<PointerAction>(static_cast<unsigned>(PointerAction::Skill1)+i),
                skillLabel(id)+" LV "+std::to_string(rank),true,
                skillDescription(id,rank,h.campaign.skillRules));
        }
    } else if(ended) {
        result.heading=h.outcome==sim::MatchOutcome::Win?"VICTORY":
            h.outcome==sim::MatchOutcome::Draw?"DRAW":"GAME OVER";
        result.modalLines=buildHudLines(input);
        result.modalLines.erase(result.modalLines.begin(),result.modalLines.begin()+2);
        result.modalLines.pop_back();
        add(PointerAction::Retry,"PLAY AGAIN",true,"CHOOSE A STORE AND START A NEW RUN");
    } else if(page==PointerPage::Floors) {
        result.heading="FLOORS";
        add(PointerAction::FloorDown,"FLOOR -",input.selectedFloor>0);
        add(PointerAction::FloorUp,"FLOOR +",input.selectedFloor+1<h.campaign.slots);
        add(PointerAction::NextFloor,"NEXT FREE",input.selectedFacility.has_value());
        add(PointerAction::Ground,"GROUND");
        add(PointerAction::Back,"BACK");
    } else if(page==PointerPage::Actions) {
        result.heading="CAMPAIGN ACTIONS";
        const auto& c=h.campaign;
        const bool live=!input.isPaused;
        add(PointerAction::Image,"IMAGE",live && c.reachedPhase>=2 && !c.imageCooldown &&
            h.cashCredits>=c.imageCost,"COST "+std::to_string(c.imageCost));
        add(PointerAction::World,"NEXT WORLD",live && c.reachedPhase>=3,"VIEW ANOTHER DIMENSION");
        add(PointerAction::Invert,"INVERT",live && c.reachedPhase>=3 && input.selectedStoreChain &&
            *input.selectedStoreChain!=*h.playerChain && *input.selectedStoreChain!=sim::ChainId::Aion &&
            c.visibleDimension!=0 && input.selectedFaith==c.faithMax && !input.selectedAntiStore &&
            !c.inversionCooldown && h.cashCredits>=c.inversionCost,"COST "+std::to_string(c.inversionCost));
        const auto escapeCost=c.escapeCost+h.chains[sim::chainIndex(*h.playerChain)].buildCost;
        add(PointerAction::Escape,"ESCAPE",live && c.reachedPhase==4 && !c.escapeCooldown &&
            h.cashCredits>=escapeCost,"COST "+std::to_string(escapeCost));
        add(PointerAction::Back,"BACK");
    } else {
        const bool canBuild=!input.isPaused && input.selectionIsPlacementCandidate &&
            !input.selectedStoreChain && h.cashCredits>=input.selectedBuildCostCredits;
        add(PointerAction::Build,"BUILD",canBuild,input.selectedFacility
            ? "COST "+std::to_string(input.selectedBuildCostCredits):"SELECT A LOT",
            h.campaign.enabled && h.campaign.reachedPhase>=2);
        add(PointerAction::Cancel,"DESELECT",input.selectedFacility.has_value());
        add(PointerAction::ZoomOut,"ZOOM -");
        add(PointerAction::ZoomIn,"ZOOM +");
        add(PointerAction::Floors,"FLOORS",h.campaign.enabled && h.campaign.reachedPhase>=2,
            "FLOOR "+std::to_string(input.selectedFloor+1));
        add(PointerAction::Actions,"ACTIONS",h.campaign.enabled && h.campaign.reachedPhase>=2);
        add(PointerAction::Pause,input.isPaused?"RESUME":"PAUSE");
        add(PointerAction::Help,input.showControls?"HIDE HELP":"HELP");
    }
    const float width=static_cast<float>(extent.width),height=static_cast<float>(extent.height);
    if(result.modal) {
        const float logicalHeight=64+70*static_cast<float>(result.buttons.size())+
            20*static_cast<float>(result.modalLines.size());
        result.uiScale=std::min(result.uiScale,height/logicalHeight);
    }
    const float gap=6*result.uiScale,margin=8*result.uiScale;
    if(result.modal) {
        const float cardWidth=std::min(width-2*margin,720*result.uiScale);
        const float cardHeight=64*result.uiScale;
        const float summaryHeight=static_cast<float>(result.modalLines.size())*20*result.uiScale;
        const float headingHeight=42*result.uiScale;
        const float groupHeight=headingHeight+static_cast<float>(result.buttons.size())*(cardHeight+gap)+summaryHeight;
        const float top=(height-groupHeight)/2+headingHeight;
        for(std::size_t i=0;i<result.buttons.size();++i)
            result.buttons[i].rect={(width-cardWidth)/2,top+static_cast<float>(i)*(cardHeight+gap),cardWidth,cardHeight};
        result.panel={0,0,width,height};
    } else {
        const auto columns=std::clamp(static_cast<unsigned>((width-2*margin)/(132*result.uiScale)),2U,8U);
        const auto rows=(result.buttons.size()+columns-1)/columns;
        const float buttonWidth=(width-2*margin-(columns-1)*gap)/columns;
        const float buttonHeight=40*result.uiScale;
        const float top=height-margin-static_cast<float>(rows)*(buttonHeight+gap);
        result.panel={0,top-gap,width,height-top+gap};
        for(std::size_t i=0;i<result.buttons.size();++i)
            result.buttons[i].rect={margin+static_cast<float>(i%columns)*(buttonWidth+gap),
                top+static_cast<float>(i/columns)*(buttonHeight+gap),buttonWidth,buttonHeight};
    }
    result.statusLines=buildPointerHudLines(input);
    if(!result.statusLines.empty()) {
        auto& style=result.statusStyle;
        style.glyphPixelScale*=result.uiScale;
        style.glyphSpacingPixels*=result.uiScale;
        style.linePaddingPixels*=result.uiScale;
        float maxWidth=1;
        for(const auto& line:result.statusLines) maxWidth=std::max(maxWidth,render::hudTextWidthPixels(line,style));
        const float blockHeight=render::hudLineHeightPixels(style)*static_cast<float>(result.statusLines.size());
        const float fit=std::min({1.0F,std::max(1.0F,result.panel.y-32)/blockHeight,
            std::max(1.0F,width*0.8F-32)/maxWidth});
        style.glyphPixelScale*=fit;style.glyphSpacingPixels*=fit;style.linePaddingPixels*=fit;
        result.statusPanel={style.originXPixels-style.panelPaddingPixels,style.originYPixels-style.panelPaddingPixels,
            maxWidth*fit+2*style.panelPaddingPixels,blockHeight*fit-style.linePaddingPixels+2*style.panelPaddingPixels};
    }
    return result;
}
const PointerButton* hitPointerControl(const PointerControls& controls,const double x,const double y) noexcept {
    for(const auto& button:controls.buttons) if(button.rect.contains(x,y)) return &button;
    return nullptr;
}
}
