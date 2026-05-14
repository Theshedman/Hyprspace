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

    // general:gaps_in/gaps_out are CCssGapData (Hyprlang CUSTOMTYPE), not INT.
    // Looking them up as INT trips ConfigValue.hpp:84's
    // RASSERT("CConfigValue<Config::INTEGER> on a FUCKED type"), aborting
    // Hyprland on the first config.reloaded event after monitor registration.
    // Use the IComplexConfigValue pattern Hyprland itself uses in
    // LayoutManager.cpp.
    static auto PGAPSIN  = CConfigValue<Config::IComplexConfigValue>("general:gaps_in");
    static auto PGAPSOUT = CConfigValue<Config::IComplexConfigValue>("general:gaps_out");

    const auto* pGapsIn  = (Config::CCssGapData*)PGAPSIN.ptr();
    const auto* pGapsOut = (Config::CCssGapData*)PGAPSOUT.ptr();

    const std::string standardGapsIn  = pGapsIn  ? pGapsIn->toString()  : "0";
    const std::string standardGapsOut = pGapsOut ? pGapsOut->toString() : "0";

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
