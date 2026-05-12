#include "Overview.hpp"
#include "Globals.hpp"
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/shared/workspace/WorkspaceRuleManager.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/Compositor.hpp>

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

    // FIX: Safely retrieve layout integers without breaking on null custom pointers
    static auto PGAPSIN_VALUE = CConfigValue<Hyprlang::INT>("general:gaps_in");
    static auto PGAPSOUT_VALUE = CConfigValue<Hyprlang::INT>("general:gaps_out");

    const std::string standardGapsIn = std::to_string(*PGAPSIN_VALUE);
    const std::string standardGapsOut = std::to_string(*PGAPSOUT_VALUE);

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
            auto pWs = wsRef.lock();
            if (!pWs || g_pCompositor->getWorkspaceByID(pWs->m_id) != pWs) continue;

            if (pWs->monitorID() == ownerID && pWs != oActiveWorkspace) {
                const auto rule = getRuleForWorkspace(pWs);
                
                std::string gapsInStr = rule.m_gapsIn.has_value() ? rule.m_gapsIn->toString() : standardGapsIn;
                std::string gapsOutStr = rule.m_gapsOut.has_value() ? rule.m_gapsOut->toString() : standardGapsOut;

                const auto curRules = std::to_string(pWs->m_id) + " gapsin:" + gapsInStr + ", gapsout:" + gapsOutStr;
                if (Config::overrideGaps) {
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
            auto pWs = wsRef.lock();
            if (!pWs || g_pCompositor->getWorkspaceByID(pWs->m_id) != pWs) continue;

            if (pWs->monitorID() == ownerID) {
                const auto rule = getRuleForWorkspace(pWs);
                std::string gapsInStr = rule.m_gapsIn.has_value() ? rule.m_gapsIn->toString() : standardGapsIn;
                std::string gapsOutStr = rule.m_gapsOut.has_value() ? rule.m_gapsOut->toString() : standardGapsOut;

                const auto curRules = std::to_string(pWs->m_id) + " gapsin:" + gapsInStr + ", gapsout:" + gapsOutStr;
                if (Config::overrideGaps) {
                    HyprlandAPI::invokeHyprctlCommand("keyword", "workspace " + curRules);
                }
            }
        }
        g_layoutManager->recalculateMonitor(pMonitor);
    }

    g_pHyprRenderer->damageMonitor(pMonitor);
}
