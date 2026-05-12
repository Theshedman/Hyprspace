#include "Overview.hpp"
#include "Globals.hpp"
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/managers/ConfigManager.hpp>
// Fix for member and incomplete type errors:
#include <hyprland/src/config/shared/workspace/WorkspaceRuleManager.hpp>
#include <hyprland/src/config/legacy/ConfigManager.hpp>

// Helper to query rules via the specialized Rule Manager
static SWorkspaceRule getRuleForWorkspace(PHLWORKSPACE pWorkspace) {
    if (!pWorkspace) return SWorkspaceRule{};

    // 1. Access the unified manager via g_pCompositor
    auto& configMgr = g_pCompositor->m_pConfigManager;
    
    // 2. Query the rules directly from the manager
    // In v0.55.0, both Lua and Legacy rules are merged into this internal list
    const auto& rules = configMgr->getAllWorkspaceRules();

    for (const auto& r : rules) {
        // Match by workspace name or ID
        if (r.workspaceString == pWorkspace->m_szName || r.workspaceString == std::to_string(pWorkspace->m_iID)) {
            return r;
        }
    }

    return SWorkspaceRule{};
}

// FIXME: preserve original workspace rules
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

    // Get the Legacy manager specifically for string-based rule injection (The Hack)
    auto legacyMgr = Config::Legacy::mgr().lock();
    if (!legacyMgr) return;

    if (active) {
        const auto oActiveWorkspace = pMonitor->m_activeWorkspace;
        if (!oActiveWorkspace) return;

        for (auto& wsRef : g_pCompositor->getWorkspaces()) {
            auto ws = wsRef.lock();
            // v0.55.0 Fix: Use ws->m_monitor->m_id and ws->m_id
            if (ws && ws->m_monitor->m_id == ownerID && ws != oActiveWorkspace) {
                const auto rule = getRuleForWorkspace(ws);
                
                std::string gapsInStr = rule.gapsIn.has_value() ? rule.gapsIn->toString() : PGAPSIN->toString();
                std::string gapsOutStr = rule.gapsOut.has_value() ? rule.gapsOut->toString() : PGAPSOUT->toString();

                const auto curRules = std::to_string(ws->m_id) + ", gapsin:" + gapsInStr + ", gapsout:" + gapsOutStr;
                if (Config::overrideGaps) {
                    legacyMgr->handleWorkspaceRules("", curRules);
                }
            }
        }

        const auto oActiveRule = getRuleForWorkspace(oActiveWorkspace);
        std::string activeGapsIn = oActiveRule.gapsIn.has_value() ? oActiveRule.gapsIn->toString() : std::to_string(Config::gapsIn);
        std::string activeGapsOut = oActiveRule.gapsOut.has_value() ? oActiveRule.gapsOut->toString() : std::to_string(Config::gapsOut);

        const auto curRules = std::to_string(oActiveWorkspace->m_id) + ", gapsin:" + activeGapsIn + ", gapsout:" + activeGapsOut;
        if (Config::overrideGaps) {
            legacyMgr->handleWorkspaceRules("", curRules);
        }
        
        g_layoutManager->recalculateMonitor(pMonitor);

    }
    else {
        for (auto& wsRef : g_pCompositor->getWorkspaces()) {
            auto ws = wsRef.lock();
            if (ws && ws->m_monitor->m_id == ownerID) {
                const auto rule = getRuleForWorkspace(ws);
                std::string gapsInStr = rule.gapsIn.has_value() ? rule.gapsIn->toString() : PGAPSIN->toString();
                std::string gapsOutStr = rule.gapsOut.has_value() ? rule.gapsOut->toString() : PGAPSOUT->toString();

                const auto curRules = std::to_string(ws->m_id) + ", gapsin:" + gapsInStr + ", gapsout:" + gapsOutStr;
                if (Config::overrideGaps) {
                    legacyMgr->handleWorkspaceRules("", curRules);
                }
            }
        }
        g_layoutManager->recalculateMonitor(pMonitor);
    }

    g_pHyprRenderer->damageMonitor(pMonitor);
}
