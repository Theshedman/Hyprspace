#include <expected>

#include <hyprland/src/plugins/PluginSystem.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/devices/IPointer.hpp>
#include <hyprland/src/devices/IKeyboard.hpp>
#include <hyprland/src/devices/ITouch.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include "Overview.hpp"
#include "Globals.hpp"
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/legacy/ConfigManager.hpp>
#include <hyprland/src/config/lua/ConfigManager.hpp>
//#include <hyprland/src/debug/Log.hpp> 
#include <hyprland/src/config/values/types/IntValue.hpp>
#include <hyprland/src/config/values/types/FloatValue.hpp>
#include <hyprland/src/config/values/types/StringValue.hpp>
#include <hyprland/src/config/values/types/IValue.hpp>



void* pRenderWindow;
void* pRenderLayer;

std::vector<std::shared_ptr<CHyprspaceWidget>> g_overviewWidgets;


CHyprColor Config::panelBaseColor = CHyprColor(0, 0, 0, 0);
CHyprColor Config::panelBorderColor = CHyprColor(0, 0, 0, 0);
CHyprColor Config::workspaceActiveBackground = CHyprColor(0, 0, 0, 0.25);
CHyprColor Config::workspaceInactiveBackground = CHyprColor(0, 0, 0, 0.5);
CHyprColor Config::workspaceActiveBorder = CHyprColor(1, 1, 1, 0.3);
CHyprColor Config::workspaceInactiveBorder = CHyprColor(1, 1, 1, 0);

int Config::panelHeight = 250;
int Config::panelBorderWidth = 2;
int Config::workspaceMargin = 12;
int Config::reservedArea = 0;
int Config::workspaceBorderSize = 1;
bool Config::adaptiveHeight = false; // TODO: implement
bool Config::centerAligned = true;
bool Config::onBottom = true; // TODO: implement
bool Config::hideBackgroundLayers = false;
bool Config::hideTopLayers = false;
bool Config::hideOverlayLayers = false;
bool Config::drawActiveWorkspace = true;
bool Config::hideRealLayers = true;
bool Config::affectStrut = true;

bool Config::overrideGaps = true;
int Config::gapsIn = 20;
int Config::gapsOut = 60;

bool Config::autoDrag = true;
bool Config::autoScroll = true;
bool Config::exitOnClick = true;
bool Config::switchOnDrop = false;
bool Config::exitOnSwitch = false;
bool Config::showNewWorkspace = true;
bool Config::showEmptyWorkspace = true;
bool Config::showSpecialWorkspace = false;

bool Config::disableGestures = false;
bool Config::reverseSwipe = false;

bool Config::disableBlur = false;

float Config::overrideAnimSpeed = 0;

float Config::dragAlpha = 0.2;

int numWorkspaces = -1; //hyprsplit/split-monitor-workspaces support

// Event listener handles (auto-unregister when destroyed)
CHyprSignalListener g_pRenderHook;
CHyprSignalListener g_pConfigReloadHook;
CHyprSignalListener g_pOpenLayerHook;
CHyprSignalListener g_pCloseLayerHook;
CHyprSignalListener g_pMouseButtonHook;
CHyprSignalListener g_pMouseAxisHook;
CHyprSignalListener g_pTouchDownHook;
CHyprSignalListener g_pTouchMoveHook;
CHyprSignalListener g_pTouchUpHook;
CHyprSignalListener g_pSwipeBeginHook;
CHyprSignalListener g_pSwipeUpdateHook;
CHyprSignalListener g_pSwipeEndHook;
CHyprSignalListener g_pKeyPressHook;
CHyprSignalListener g_pSwitchWorkspaceHook;
CHyprSignalListener g_pAddMonitorHook;

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

std::shared_ptr<CHyprspaceWidget> getWidgetForMonitor(PHLMONITORREF pMonitor) {
    for (auto& widget : g_overviewWidgets) {
        if (!widget) continue;
        if (!widget->getOwner()) continue;
        if (widget->getOwner() == pMonitor) {
            return widget;
        }
    }
    return nullptr;
}

// used to enforce the layout
void refreshWidgets() {
    for (auto& widget : g_overviewWidgets) {
        if (widget != nullptr)
            if (widget->isActive())
                widget->show();
    }
}

bool g_layoutNeedsRefresh = true;

// for restroing dragged window's alpha value
float g_oAlpha = -1;

void onRender(eRenderStage renderStage) {

    // refresh layout after scheduled recalculation on monitors were carried out in renderMonitor
    if (renderStage == eRenderStage::RENDER_PRE) {
        if (g_layoutNeedsRefresh) {
            refreshWidgets();
            g_layoutNeedsRefresh = false;
        }
    }
    else if (renderStage == eRenderStage::RENDER_PRE_WINDOWS) {
        // v0.55.0 Fix: Use pMonitor (not monitor) as suggested by compiler
        const auto widget = getWidgetForMonitor(g_pHyprRenderer->m_renderData.pMonitor);
        if (widget != nullptr && widget->getOwner()) {
            // Use g_layoutManager and dragController()
            const auto dragTarget = g_layoutManager->dragController() ? g_layoutManager->dragController()->target() : nullptr;
            const auto curWindow = dragTarget ? dragTarget->window() : nullptr;
            if (curWindow && widget->isActive()) {
                g_oAlpha = curWindow->effectiveAlpha();
                curWindow->alpha(Desktop::View::WINDOW_ALPHA_ACTIVE)->setValueAndWarp(0);
            } else g_oAlpha = -1;
        } else g_oAlpha = -1;
    }
    else if (renderStage == eRenderStage::RENDER_POST_WINDOWS) {
        const auto widget = getWidgetForMonitor(g_pHyprRenderer->m_renderData.pMonitor);
        if (widget != nullptr && widget->getOwner()) {
            widget->draw();
            if (g_oAlpha != -1) {
                const auto dragTarget = g_layoutManager->dragController() ? g_layoutManager->dragController()->target() : nullptr;
                const auto curWindow = dragTarget ? dragTarget->window() : nullptr;
                if (curWindow) {
                    curWindow->alpha(Desktop::View::WINDOW_ALPHA_ACTIVE)->setValueAndWarp(0);
                    const auto time = Time::steadyNow();
                    (*(tRenderWindow)pRenderWindow)(g_pHyprRenderer.get(), curWindow, widget->getOwner(), time, true, Render::RENDER_PASS_MAIN, false, false);
                    curWindow->alpha(Desktop::View::WINDOW_ALPHA_ACTIVE)->setValueAndWarp(0);
                }
            }
            g_oAlpha = -1;
        }
    }
}

// event hook, currently this is only here to re-hide top layer panels on workspace change
void onWorkspaceChange(PHLWORKSPACE pWorkspace) {

    if (!pWorkspace) return;

    auto widget = getWidgetForMonitor(g_pCompositor->getMonitorFromID(pWorkspace->m_monitor->m_id));
    if (widget != nullptr)
        if (widget->isActive())
            widget->show();
}

// event hook for click and drag interaction
void onMouseButton(const IPointer::SButtonEvent& event, SCallbackInfo& info) {
    const SP<IPointer> pointer = g_pSeatManager->m_mouse.lock();
    if (!pointer)
        return;

    if (event.button != BTN_LEFT) return;

    const auto pressed = event.state == WL_POINTER_BUTTON_STATE_PRESSED;
    const auto pMonitor = g_pCompositor->getMonitorFromCursor();
    if (pMonitor) {
        const auto widget = getWidgetForMonitor(pMonitor);
        if (widget) {
            if (widget->isActive()) {
                info.cancelled = !widget->buttonEvent(pressed, g_pInputManager->getMouseCoordsInternal());
            }
        }
    }

}

// event hook for scrolling through panel and workspaces
void onMouseAxis(const IPointer::SAxisEvent& event, SCallbackInfo& info) {

    const auto pMonitor = g_pCompositor->getMonitorFromCursor();
    if (pMonitor) {
        const auto widget = getWidgetForMonitor(pMonitor);
        if (widget) {
            if (widget->isActive()) {
                info.cancelled = !widget->axisEvent(event.delta, event.axis, g_pInputManager->getMouseCoordsInternal());
            }
        }
    }

}

// event hook for swipe
void onSwipeBegin(const IPointer::SSwipeBeginEvent& event, SCallbackInfo& info) {

    if (Config::disableGestures) return;

    const auto widget = getWidgetForMonitor(g_pCompositor->getMonitorFromCursor());
    
    if (widget != nullptr)
        widget->beginSwipe(event);

    // end other widget swipe
    for (auto& w : g_overviewWidgets) {
        if (w != widget && w->isSwiping()) {
            IPointer::SSwipeEndEvent dummy;
            dummy.cancelled = true;
            w->endSwipe(dummy);
        }
    }
}

// event hook for update swipe, most of the swiping mechanics are here
void onSwipeUpdate(const IPointer::SSwipeUpdateEvent& event, SCallbackInfo& info) {

    if (Config::disableGestures) return;

    const auto widget = getWidgetForMonitor(g_pCompositor->getMonitorFromCursor());
    if (widget != nullptr)
        info.cancelled = !widget->updateSwipe(event);
}

// event hook for end swipe
void onSwipeEnd(const IPointer::SSwipeEndEvent& event, SCallbackInfo& info) {

    if (Config::disableGestures) return;

    const auto widget = getWidgetForMonitor(g_pCompositor->getMonitorFromCursor());
    if (widget != nullptr)
        widget->endSwipe(event);
}

// Close overview with configurable key
void onKeyPress(const IKeyboard::SKeyEvent& event, SCallbackInfo& info) {
    const SP<IKeyboard> keyboard = g_pSeatManager->m_keyboard.lock();
    if (!keyboard || !keyboard->m_xkbSymState)
        return;

    const auto keycode = event.keycode + 8; // Offset for xkbcommon
    const xkb_keysym_t keysym = xkb_state_key_get_one_sym(keyboard->m_xkbSymState, keycode);

    // 1. Get the CStringValue from your saved pointer
    const auto reply = (Config::Values::CStringValue*)ConfigPtr::exitKey.get();
    if (!reply)
        return;

    // 2. Access the value directly (replaces .dataptr and *(Hyprlang::STRING*))
    const std::string cfgExitKey = reply->value();
    if (cfgExitKey.empty())
        return;

    // 3. Convert string to keysym
    const xkb_keysym_t cfgExitKeysym = xkb_keysym_from_name(cfgExitKey.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);

    if (keysym == cfgExitKeysym) {
        bool overviewActive = false;
        for (auto& widget : g_overviewWidgets) {
            if (widget != nullptr && widget->isActive()) {
                widget->hide();
                overviewActive = true;
            }
        }
        
        // 4. Cancel event using the new SCallbackInfo structure
        if (overviewActive)
            info.cancelled = true;
    }
}

PHLMONITOR g_pTouchedMonitor;

void onTouchDown(const ITouch::SDownEvent& event, SCallbackInfo& info) {
    if (!event.device)
        return;

    auto targetMonitor = g_pCompositor->getMonitorFromName(!event.device->m_boundOutput.empty() ? event.device->m_boundOutput : "");
    targetMonitor = targetMonitor ? targetMonitor : g_pCompositor->getMonitorFromCursor();

    const auto widget = getWidgetForMonitor(targetMonitor);
    if (widget != nullptr && targetMonitor.get() != nullptr) {
        if (widget->isActive()) {
            // v0.55.0 Fix: Use m_position and m_size
            Vector2D pos = targetMonitor->m_position + event.pos * targetMonitor->m_size;
            info.cancelled = !widget->buttonEvent(true, pos);
            if (info.cancelled) {
                g_pTouchedMonitor = targetMonitor;
                g_pCompositor->warpCursorTo(pos);
                g_pInputManager->refocus();
            }
        }
    }
}

void onTouchMove(const ITouch::SMotionEvent& event, SCallbackInfo& info) {
    if (g_pTouchedMonitor == nullptr) return;

    // v0.55.0 Fix: Use m_position and m_size
    g_pCompositor->warpCursorTo(g_pTouchedMonitor->m_position + g_pTouchedMonitor->m_size * event.pos);
    g_pInputManager->simulateMouseMovement();
}


void onTouchUp(const ITouch::SUpEvent& event, SCallbackInfo& info) {
    const auto widget = getWidgetForMonitor(g_pTouchedMonitor);
    if (widget != nullptr && g_pTouchedMonitor.get() != nullptr)
        if (widget->isActive())
            info.cancelled = !widget->buttonEvent(false, g_pInputManager->getMouseCoordsInternal());

    g_pTouchedMonitor = nullptr;
}

static SDispatchResult dispatchToggleOverview(std::string arg) {
    auto currentMonitor = g_pCompositor->getMonitorFromCursor();
    auto widget = getWidgetForMonitor(currentMonitor);
    if (widget) {
        if (arg.find("all") != std::string::npos) {
            if (widget->isActive()) {
                for (auto& widget : g_overviewWidgets) {
                    if (widget != nullptr)
                        if (widget->isActive())
                            widget->hide();
                }
            }
            else {
                for (auto& widget : g_overviewWidgets) {
                    if (widget != nullptr)
                        if (!widget->isActive())
                            widget->show();
                }
            }
        }
        else
            widget->isActive() ? widget->hide() : widget->show();
    }
    return SDispatchResult{};
}

static SDispatchResult dispatchOpenOverview(std::string arg) {
    if (arg.find("all") != std::string::npos) {

        for (auto& widget : g_overviewWidgets) {
            if (!widget->isActive()) widget->show();
        }
    }
    else {
        auto currentMonitor = g_pCompositor->getMonitorFromCursor();
        auto widget = getWidgetForMonitor(currentMonitor);
        if (widget)
            if (!widget->isActive()) widget->show();
    }
    return SDispatchResult{};
}

static SDispatchResult dispatchCloseOverview(std::string arg) {
    if (arg.find("all") != std::string::npos) {

        for (auto& widget : g_overviewWidgets) {
            if (widget->isActive()) widget->hide();
        }
    }
    else {
        auto currentMonitor = g_pCompositor->getMonitorFromCursor();
        auto widget = getWidgetForMonitor(currentMonitor);
        if (widget)
            if (widget->isActive()) widget->hide();
    }
    return SDispatchResult{};
}

void* findFunctionBySymbol(HANDLE inHandle, const std::string func, const std::string sym) {
    auto funcSearch = HyprlandAPI::findFunctionsByName(inHandle, func);
    for (auto f : funcSearch) {
        if (f.demangled.find(sym) != std::string::npos)
            return f.address;
    }
    return nullptr;
}
void reloadConfig() {
    auto getInt = [&](const SP<Config::Values::IValue>& ptr) -> int64_t {
        return ptr ? ((Config::Values::CIntValue*)ptr.get())->value() : 0;
    };

    auto getFloat = [&](const SP<Config::Values::IValue>& ptr) -> float {
        return ptr ? ((Config::Values::CFloatValue*)ptr.get())->value() : 0.0f;
    };

    // 2. Map Colors (Stored as Ints using hex)
    Config::panelBaseColor              = CHyprColor((uint64_t)getInt(ConfigPtr::panelColor));
    Config::panelBorderColor            = CHyprColor((uint64_t)getInt(ConfigPtr::panelBorderColor));
    Config::workspaceActiveBackground   = CHyprColor((uint64_t)getInt(ConfigPtr::workspaceActiveBackground));
    Config::workspaceInactiveBackground = CHyprColor((uint64_t)getInt(ConfigPtr::workspaceInactiveBackground));
    Config::workspaceActiveBorder       = CHyprColor((uint64_t)getInt(ConfigPtr::workspaceActiveBorder));
    Config::workspaceInactiveBorder     = CHyprColor((uint64_t)getInt(ConfigPtr::workspaceInactiveBorder));

    // 3. Map Integers / Bools
    Config::panelHeight            = getInt(ConfigPtr::panelHeight);
    Config::panelBorderWidth       = getInt(ConfigPtr::panelBorderWidth);
    Config::workspaceMargin        = getInt(ConfigPtr::workspaceMargin);
    Config::reservedArea           = getInt(ConfigPtr::reservedArea);
    Config::workspaceBorderSize    = getInt(ConfigPtr::workspaceBorderSize);
    Config::adaptiveHeight         = getInt(ConfigPtr::adaptiveHeight);
    Config::centerAligned          = getInt(ConfigPtr::centerAligned);
    Config::onBottom               = getInt(ConfigPtr::onBottom);
    Config::hideBackgroundLayers   = getInt(ConfigPtr::hideBackgroundLayers);
    Config::hideTopLayers          = getInt(ConfigPtr::hideTopLayers);
    Config::hideOverlayLayers      = getInt(ConfigPtr::hideOverlayLayers);
    Config::drawActiveWorkspace    = getInt(ConfigPtr::drawActiveWorkspace);
    Config::hideRealLayers         = getInt(ConfigPtr::hideRealLayers);
    Config::affectStrut            = getInt(ConfigPtr::affectStrut);
    Config::overrideGaps           = getInt(ConfigPtr::overrideGaps);
    Config::gapsIn                 = getInt(ConfigPtr::gapsIn);
    Config::gapsOut                = getInt(ConfigPtr::gapsOut);
    Config::autoDrag               = getInt(ConfigPtr::autoDrag);
    Config::autoScroll             = getInt(ConfigPtr::autoScroll);
    Config::exitOnClick            = getInt(ConfigPtr::exitOnClick);
    Config::switchOnDrop           = getInt(ConfigPtr::switchOnDrop);
    Config::exitOnSwitch           = getInt(ConfigPtr::exitOnSwitch);
    Config::showNewWorkspace       = getInt(ConfigPtr::showNewWorkspace);
    Config::showEmptyWorkspace     = getInt(ConfigPtr::showEmptyWorkspace);
    Config::showSpecialWorkspace   = getInt(ConfigPtr::showSpecialWorkspace);
    Config::disableGestures        = getInt(ConfigPtr::disableGestures);
    Config::reverseSwipe           = getInt(ConfigPtr::reverseSwipe);
    Config::disableBlur            = getInt(ConfigPtr::disableBlur);

    Config::overrideAnimSpeed      = getFloat(ConfigPtr::overrideAnimSpeed);
    Config::dragAlpha              = getFloat(ConfigPtr::dragAlpha);

    for (auto& widget : g_overviewWidgets) {
        if (widget) {
            widget->updateConfig();
            widget->hide();
            IPointer::SSwipeEndEvent dummy;
            dummy.cancelled = true;
            widget->endSwipe(dummy);
        }
    }

    // safely look up multi-workspace management plugin extensions
    auto numWorkspacesConfig = HyprlandAPI::getConfigValue(pHandle, "plugin:hyprsplit:num_workspaces");
    if (!numWorkspacesConfig || !numWorkspacesConfig->dataPtr())
        numWorkspacesConfig = HyprlandAPI::getConfigValue(pHandle, "plugin:split-monitor-workspaces:count");
        
    if (numWorkspacesConfig && numWorkspacesConfig->dataPtr())
        numWorkspaces = *(int64_t*)numWorkspacesConfig->dataPtr();

    // TODO: schedule frame for monitor?
}

void registerMonitors() {
    // create a widget for each monitor
    for (auto& m : g_pCompositor->m_monitors) {
        if (!m || getWidgetForMonitor(m) != nullptr) continue;
        CHyprspaceWidget* widget = new CHyprspaceWidget(m->m_id);
        g_overviewWidgets.emplace_back(widget);
    }
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE inHandle) {
    pHandle = inHandle;

    Log::logger->log(Log::DEBUG, "Loading overview plugin");
    // Colors - Stored in ConfigPtr for live access
    ConfigPtr::panelColor = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:panelColor", "Panel background color", (int64_t)CHyprColor(0, 0, 0, 0).getAsHex(), Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::panelColor);

    ConfigPtr::panelBorderColor = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:panelBorderColor", "Panel border color", (int64_t)CHyprColor(0, 0, 0, 0).getAsHex(), Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::panelBorderColor);

    ConfigPtr::workspaceActiveBackground = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:workspaceActiveBackground", "Active workspace background", (int64_t)CHyprColor(0, 0, 0, 0.25).getAsHex(), Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::workspaceActiveBackground);

    ConfigPtr::workspaceInactiveBackground = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:workspaceInactiveBackground", "Inactive workspace background", (int64_t)CHyprColor(0, 0, 0, 0.5).getAsHex(), Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::workspaceInactiveBackground);

    ConfigPtr::workspaceActiveBorder = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:workspaceActiveBorder", "Active workspace border", (int64_t)CHyprColor(1, 1, 1, 0.25).getAsHex(), Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::workspaceActiveBorder);

    ConfigPtr::workspaceInactiveBorder = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:workspaceInactiveBorder", "Inactive workspace border", (int64_t)CHyprColor(1, 1, 1, 0).getAsHex(), Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::workspaceInactiveBorder);

    // Integers / Booleans
    ConfigPtr::panelHeight = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:panelHeight", "Overview panel height", (int64_t)250, Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::panelHeight);

    ConfigPtr::panelBorderWidth = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:panelBorderWidth", "Panel border thickness", (int64_t)2, Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::panelBorderWidth);

    ConfigPtr::workspaceMargin = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:workspaceMargin", "Margin between workspaces", (int64_t)12, Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::workspaceMargin);

    ConfigPtr::workspaceBorderSize = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:workspaceBorderSize", "Workspace border thickness", (int64_t)1, Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::workspaceBorderSize);

    ConfigPtr::reservedArea = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:reservedArea", "Reserved area size", (int64_t)0, Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::reservedArea);

    ConfigPtr::adaptiveHeight = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:adaptiveHeight", "Enable adaptive height", (int64_t)0, Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::adaptiveHeight);

    ConfigPtr::centerAligned = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:centerAligned", "Center align the overview", (int64_t)1, Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::centerAligned);

    ConfigPtr::onBottom = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:onBottom", "Place overview at the bottom", (int64_t)0, Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::onBottom);

    ConfigPtr::hideBackgroundLayers = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:hideBackgroundLayers", "Hide background layers", (int64_t)0, Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::hideBackgroundLayers);

    ConfigPtr::hideTopLayers = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:hideTopLayers", "Hide top layers", (int64_t)0, Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::hideTopLayers);

    ConfigPtr::hideOverlayLayers = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:hideOverlayLayers", "Hide overlay layers", (int64_t)0, Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::hideOverlayLayers);

    ConfigPtr::drawActiveWorkspace = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:drawActiveWorkspace", "Draw active workspace", (int64_t)1, Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::drawActiveWorkspace);

    ConfigPtr::hideRealLayers = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:hideRealLayers", "Hide real layers", (int64_t)1, Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::hideRealLayers);

    ConfigPtr::affectStrut = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:affectStrut", "Affect desktop struts", (int64_t)1, Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::affectStrut);

    ConfigPtr::overrideGaps = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:overrideGaps", "Override workspace gaps", (int64_t)1, Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::overrideGaps);

    ConfigPtr::gapsIn = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:gapsIn", "Inner gaps size", (int64_t)20, Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::gapsIn);

    ConfigPtr::gapsOut = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:gapsOut", "Outer gaps size", (int64_t)60, Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::gapsOut);

    ConfigPtr::autoDrag = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:autoDrag", "Enable auto-drag", (int64_t)1, Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::autoDrag);

    ConfigPtr::autoScroll = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:autoScroll", "Enable auto-scroll", (int64_t)1, Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::autoScroll);

    ConfigPtr::exitOnClick = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:exitOnClick", "Exit overview on click", (int64_t)1, Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::exitOnClick);

    ConfigPtr::switchOnDrop = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:switchOnDrop", "Switch workspace on drop", (int64_t)0, Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::switchOnDrop);

    ConfigPtr::exitOnSwitch = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:exitOnSwitch", "Exit overview on switch", (int64_t)0, Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::exitOnSwitch);

    ConfigPtr::showNewWorkspace = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:showNewWorkspace", "Show new workspace button", (int64_t)1, Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::showNewWorkspace);

    ConfigPtr::showEmptyWorkspace = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:showEmptyWorkspace", "Show empty workspaces", (int64_t)1, Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::showEmptyWorkspace);

    ConfigPtr::showSpecialWorkspace = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:showSpecialWorkspace", "Show special workspace", (int64_t)0, Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::showSpecialWorkspace);

    ConfigPtr::disableGestures = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:disableGestures", "Disable overview gestures", (int64_t)1, Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::disableGestures);

    ConfigPtr::reverseSwipe = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:reverseSwipe", "Reverse swipe direction", (int64_t)0, Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::reverseSwipe);

    ConfigPtr::disableBlur = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:overview:disableBlur", "Disable blur in overview", (int64_t)0, Config::Values::SIntValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::disableBlur);

    // Floats
    ConfigPtr::overrideAnimSpeed = Hyprutils::Memory::makeShared<Config::Values::CFloatValue>("plugin:overview:overrideAnimSpeed", "Override animation speed", 0.0f, Config::Values::SFloatValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::overrideAnimSpeed);

    ConfigPtr::dragAlpha = Hyprutils::Memory::makeShared<Config::Values::CFloatValue>("plugin:overview:dragAlpha", "Drag transparency alpha", 0.2f, Config::Values::SFloatValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::dragAlpha);

    // String
    ConfigPtr::exitKey = Hyprutils::Memory::makeShared<Config::Values::CStringValue>("plugin:overview:exitKey", "Key to exit overview", "Escape", Config::Values::SStringValueOptions{});
    HyprlandAPI::addConfigValueV2(pHandle, ConfigPtr::exitKey);

    g_pRenderHook = Event::bus()->m_events.render.stage.listen(onRender);

    // refresh on layer change
    g_pOpenLayerHook = Event::bus()->m_events.layer.opened.listen([](const PHLLS&) { g_layoutNeedsRefresh = true; });
    g_pCloseLayerHook = Event::bus()->m_events.layer.closed.listen([](const PHLLS&) { g_layoutNeedsRefresh = true; });

    g_pMouseButtonHook = listenCancellable<IPointer::SButtonEvent>(Event::bus()->m_events.input.mouse.button, onMouseButton);
    g_pMouseAxisHook = listenCancellable<IPointer::SAxisEvent>(Event::bus()->m_events.input.mouse.axis, onMouseAxis);

    g_pTouchDownHook = listenCancellable<ITouch::SDownEvent>(Event::bus()->m_events.input.touch.down, onTouchDown);
    g_pTouchMoveHook = listenCancellable<ITouch::SMotionEvent>(Event::bus()->m_events.input.touch.motion, onTouchMove);
    g_pTouchUpHook = listenCancellable<ITouch::SUpEvent>(Event::bus()->m_events.input.touch.up, onTouchUp);

    g_pSwipeBeginHook = listenCancellable<IPointer::SSwipeBeginEvent>(Event::bus()->m_events.gesture.swipe.begin, onSwipeBegin);
    g_pSwipeUpdateHook = listenCancellable<IPointer::SSwipeUpdateEvent>(Event::bus()->m_events.gesture.swipe.update, onSwipeUpdate);
    g_pSwipeEndHook = listenCancellable<IPointer::SSwipeEndEvent>(Event::bus()->m_events.gesture.swipe.end, onSwipeEnd);

    g_pKeyPressHook = listenCancellable<IKeyboard::SKeyEvent>(Event::bus()->m_events.input.keyboard.key, onKeyPress);

    g_pSwitchWorkspaceHook = Event::bus()->m_events.workspace.active.listen(onWorkspaceChange);

    pRenderWindow = findFunctionBySymbol(pHandle, "renderWindow", "IHyprRenderer::renderWindow");
    if (!pRenderWindow)
        pRenderWindow = findFunctionBySymbol(pHandle, "renderWindow", "CHyprRenderer::renderWindow");
    pRenderLayer = findFunctionBySymbol(pHandle, "renderLayer", "IHyprRenderer::renderLayer");
    if (!pRenderLayer)
        pRenderLayer = findFunctionBySymbol(pHandle, "renderLayer", "CHyprRenderer::renderLayer");

    registerMonitors();
    g_pAddMonitorHook = Event::bus()->m_events.monitor.added.listen([](const PHLMONITOR&) { registerMonitors(); });

    return {"Hyprspace", "Workspace overview", "KZdkm", "0.1"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    g_pRenderHook.reset();
    g_pConfigReloadHook.reset();
    g_pOpenLayerHook.reset();
    g_pCloseLayerHook.reset();
    g_pMouseButtonHook.reset();
    g_pMouseAxisHook.reset();
    g_pTouchDownHook.reset();
    g_pTouchMoveHook.reset();
    g_pTouchUpHook.reset();
    g_pSwipeBeginHook.reset();
    g_pSwipeUpdateHook.reset();
    g_pSwipeEndHook.reset();
    g_pKeyPressHook.reset();
    g_pSwitchWorkspaceHook.reset();
    g_pAddMonitorHook.reset();

    g_overviewWidgets.clear();

    pRenderWindow = nullptr;
    pRenderLayer = nullptr;
    pHandle = nullptr;
}
