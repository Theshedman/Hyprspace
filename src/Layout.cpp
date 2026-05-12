#include "Overview.hpp"
#include "Globals.hpp"
#include <hyprland/src/config/legacy/ConfigManager.hpp>

// FIXME: preserve original workspace rules
void CHyprspaceWidget::updateLayout() {

    if (!Config::affectStrut) return;

    const auto pMonitor = getOwner();
    if (!pMonitor) return;

    const auto currentHeight = Config::panelHeight + Config::reservedArea;

    // ensure custom types are handled through the new Hyprlang pointers safely
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

    g_pHyprRenderer->arrangeLayersForMonitor(pMonitor->m_id);

    // gaps are created via workspace rules
    // there are no way to write to m_dWorkspaceRules directly
    // and we want to refrain from using function hooks
    // so we create a workspace rule for ALL workspaces through handleWorkspaceRules
    // Geneva Convention violation type hack but idc atm
    if (active) {
        // guard against null active workspace during reload
        const auto oActiveWorkspace = pMonitor->activeWorkspace;
        if (!oActiveWorkspace) return;

        for (auto& ws : g_pCompositor->m_vWorkspaces) { 
            if (ws && ws->m_iMonitorID == pMonitor->m_id && ws != oActiveWorkspace) {
                
                // Safety: Don't swap active workspace pointers during a layout update if they are invalid
                const auto curRules = std::to_string(ws->m_iID) + ", gapsin:" + PGAPSIN->toString() + ", gapsout:" + PGAPSOUT->toString();
                
                if (Config::overrideGaps) {
                    if (const auto legacy = Config::Legacy::mgr().lock())
                        legacy->handleWorkspaceRules("", curRules);
                }
            }
        }
        
        // recalculate only if we have a valid monitor handle
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(pMonitor->m_id);

    } else {
        for (auto& ws : g_pCompositor->m_vWorkspaces) {
            if (ws && ws->m_iMonitorID == pMonitor->m_id) {
                const auto curRules = std::to_string(ws->m_iID) + ", gapsin:" + PGAPSIN->toString() + ", gapsout:" + PGAPSOUT->toString();
                if (Config::overrideGaps) {
                    if (const auto legacy = Config::Legacy::mgr().lock())
                        legacy->handleWorkspaceRules("", curRules);
                }
            }
        }
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(pMonitor->m_id);
    }
}
