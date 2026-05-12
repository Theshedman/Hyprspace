#include "Overview.hpp"
#include "Globals.hpp"
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/shared/workspace/WorkspaceRuleManager.hpp>
#include <hyprland/src/desktop/Workspace.hpp>

// Update the return type to include the Config:: namespace wrapper
static Config::CWorkspaceRule getRuleForWorkspace(PHLWORKSPACE pWorkspace) {
    if (!pWorkspace) 
        return Config::CWorkspaceRule{};

    auto optRule = Config::workspaceRuleMgr()->getWorkspaceRuleFor(pWorkspace);

    return optRule.has_value() ? optRule.value() : Config::CWorkspaceRule{};
}




void CHyprspaceWidget::updateLayout() {
    if (!Config::affectStrut) return;

    const auto pMonitor = getOwner();
    if (!pMonitor) return;

    const auto currentHeight = Config::panelHeight + Config::reservedArea;

    static auto PGAPSINDATA = CConfigValue<Hyprlang::CUSTOMTYPE>("general:gaps_in");
    static auto PGAPSOUTDATA = CConfigValue<Hyprlang::CUSTOMTYPE>("general:gaps_out");
    
    auto* const PGAPSIN = (Config::CCssGapData*)(PGAPSINDATA.ptr())->getData();
    auto* const PGAPSOUT = (Config::CCssGapData*)(PGAPSOUTDATA.ptr())->getData();

    if (active) {
        if (!Config::onBottom)
            pMonitor->m_reservedArea = Desktop::CReservedArea(currentHeight, 0, 0, 0);
        else
            pMonitor->m_reservedArea = Desktop::CReservedArea(0, 0, currentHeight, 0);
    } else {
        pMonitor->m_reservedArea = Desktop::CReservedArea();
    }

    g_pHyprRenderer->arrangeLayersForMonitor(ownerID);

    if (active) {
        const auto oActiveWorkspace = pMonitor->m_activeWorkspace;
        if (!oActiveWorkspace) return;

        for (auto& wsRef : g_pCompositor->getWorkspaces()) {
            auto ws = wsRef.lock();
            if (ws && ws->m_monitor && ws->m_monitor->m_id == ownerID && ws != oActiveWorkspace) {
                const auto rule = getRuleForWorkspace(ws);
                
                std::string gapsInStr = rule.m_gapsIn.has_value() ? rule.m_gapsIn->toString() : PGAPSIN->toString();
                std::string gapsOutStr = rule.m_gapsOut.has_value() ? rule.m_gapsOut->toString() : PGAPSOUT->toString();

                const auto curRules = std::to_string(ws->m_id) + " gapsin:" + gapsInStr + ", gapsout:" + gapsOutStr;
                if (Config::overrideGaps) {
                    // Use the safe public plugin utility to execute commands down the runtime pipeline
                    HyprlandAPI::invokeHyprctlCommand("keyword", "workspace " + curRules);
                }
            }
        }

        const auto oActiveRule = getRuleForWorkspace(oActiveWorkspace);
        std::string activeGapsIn = oActiveRule.m_gapsIn.has_value() ? oActiveRule.m_gapsIn->toString() : std::to_string(Config::gapsIn);
        std::string activeGapsOut = oActiveRule.m_gapsOut.has_value() ? oActiveRule.m_gapsOut->toString() : std::to_string(Config::gapsOut);

        const auto curRules = std::to_string(oActiveWorkspace->m_id) + " gapsin:" + activeGapsIn + ", gapsout:" + activeGapsOut;
        if (Config::overrideGaps) {
            HyprlandAPI::invokeHyprctlCommand("keyword", "workspace " + curRules);
        }
        
        g_layoutManager->recalculateMonitor(pMonitor);
    }
    else {
        for (auto& wsRef : g_pCompositor->getWorkspaces()) {
            auto ws = wsRef.lock();

            if (ws && ws->m_monitor && ws->m_monitor->m_id == ownerID) {
                const auto rule = getRuleForWorkspace(ws);
                std::string gapsInStr = rule.m_gapsIn.has_value() ? rule.m_gapsIn->toString() : PGAPSIN->toString();
                std::string gapsOutStr = rule.m_gapsOut.has_value() ? rule.m_gapsOut->toString() : PGAPSOUT->toString();

                const auto curRules = std::to_string(ws->m_id) + " gapsin:" + gapsInStr + ", gapsout:" + gapsOutStr;
                if (Config::overrideGaps) {
                    HyprlandAPI::invokeHyprctlCommand("keyword", "workspace " + curRules);
                }
            }
        }
        g_layoutManager->recalculateMonitor(pMonitor);
    }

    g_pHyprRenderer->damageMonitor(pMonitor);
}
