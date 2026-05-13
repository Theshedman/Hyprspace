#include "Overview.hpp"
#include "Globals.hpp"
#include <hyprland/src/helpers/memory/Memory.hpp>
#include <hyprland/src/config/shared/complex/ComplexDataTypes.hpp>
#include <hyprland/src/render/pass/RectPassElement.hpp>
#include <hyprland/src/render/pass/BorderPassElement.hpp>
#include <hyprland/src/render/pass/RendererHintsPassElement.hpp>
#include <hyprlang.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
#include <algorithm>
#include <climits>


void renderRect(CBox box, CHyprColor color) {
    CRectPassElement::SRectData rectdata;
    rectdata.color = color;
    rectdata.box = box;
    g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(rectdata));
}

void renderRectWithBlur(CBox box, CHyprColor color) {
    CRectPassElement::SRectData rectdata;
    rectdata.color = color;
    rectdata.box = box;
    rectdata.blur = true;
    g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(rectdata));
}

void renderBorder(CBox box, const Config::CGradientValueData& gradient, int size) {
    CBorderPassElement::SBorderData data;
    data.box = box;
    data.grad1 = gradient;
    data.round = 0;
    data.a = 1.f;
    data.borderSize = size;
    g_pHyprRenderer->m_renderPass.add(makeUnique<CBorderPassElement>(data));
}

void renderWindowStub(PHLWINDOW pWindow, PHLMONITOR pMonitor, PHLWORKSPACE pWorkspaceOverride, CBox rectOverride, const Time::steady_tp& time) {
    if (!pWindow || !pMonitor || !pWorkspaceOverride) return;

    const auto oWorkspace = pWindow->m_workspace;
    const auto oFullscreen = pWindow->m_fullscreenState;
    const auto oUseNearestNeighbor = pWindow->m_ruleApplicator->nearestNeighbor();
    const auto oPinned = pWindow->m_pinned;
    const auto oFloating = pWindow->m_isFloating;
    const Vector2D oRealPosValue  = pWindow->m_realPosition->value();
    const Vector2D oRealPosGoal   = pWindow->m_realPosition->goal();
    const Vector2D oRealSizeValue = pWindow->m_realSize->value();
    const Vector2D oRealSizeGoal  = pWindow->m_realSize->goal();

    pWindow->m_workspace = pWorkspaceOverride;
    pWindow->m_fullscreenState = Desktop::View::SFullscreenState{FSMODE_NONE};
    pWindow->m_ruleApplicator->nearestNeighbor().set(false, Desktop::Types::PRIORITY_SET_PROP);
    pWindow->m_isFloating = false;
    pWindow->m_pinned = true;

    // Pin m_realPosition/m_realSize directly to the thumbnail rect (in screen
    // coordinates) instead of using a renderModif translate+scale to relocate
    // a normally-positioned texture. The reason: ElementRenderer.cpp:318 sends
    // damage = m_renderData.damage ∩ getTexBox() into the renderTexture call,
    // and getTexBox() is the texture's *un-modif'd* box. A renderModif moves
    // the actual pixel-write location, but the damage stays at the original
    // box — so the renderModif-relocated pixels are outside the damage region
    // and get culled. Pinning here makes getTexBox() equal the thumbnail rect,
    // so damage covers it and the texture actually gets written.
    pWindow->m_realPosition->setValueAndWarp(rectOverride.pos() + pMonitor->m_position);
    pWindow->m_realSize->setValueAndWarp(rectOverride.size());

    // standalone=true (below) forces renderdata.alpha/fadeAlpha to 1, but
    // SurfacePassElement also multiplies in the surface's m_alphaModifier
    // and m_overallOpacity. Pin those to 1 too so client-side opacity
    // settings don't make the thumbnail invisible against the wallpaper.
    const auto wlSurf = pWindow->wlSurface();
    const float oAlphaModifier  = wlSurf ? wlSurf->m_alphaModifier  : 1.F;
    const float oOverallOpacity = wlSurf ? wlSurf->m_overallOpacity : 1.F;
    if (wlSurf) {
        wlSurf->m_alphaModifier  = 1.F;
        wlSurf->m_overallOpacity = 1.F;
    }

    g_pHyprRenderer->damageWindow(pWindow);

    // standalone=true (last arg) forces renderdata.alpha/fadeAlpha to 1 in
    // Renderer.cpp:587-590, bypassing every window/workspace alpha multiplier.
    // Hyprland's own decorations and rounding are also skipped, which is fine
    // — the overview draws its own selection border.
    (*(tRenderWindow)pRenderWindow)(g_pHyprRenderer.get(), pWindow, pMonitor, time, true, Render::RENDER_PASS_ALL, false, true);

    // restore values for normal window render
    pWindow->m_workspace = oWorkspace;
    pWindow->m_fullscreenState = oFullscreen;
    pWindow->m_isFloating = oFloating;
    pWindow->m_pinned = oPinned;
    pWindow->m_realPosition->setValueAndWarp(oRealPosValue);
    pWindow->m_realSize->setValueAndWarp(oRealSizeValue);
    if (oRealPosValue != oRealPosGoal)
        *pWindow->m_realPosition = oRealPosGoal;
    if (oRealSizeValue != oRealSizeGoal)
        *pWindow->m_realSize = oRealSizeGoal;
    if (wlSurf) {
        wlSurf->m_alphaModifier  = oAlphaModifier;
        wlSurf->m_overallOpacity = oOverallOpacity;
    }
}

void renderLayerStub(PHLLS pLayer, PHLMONITOR pMonitor, CBox rectOverride, const Time::steady_tp& time) {
    if (!pLayer || !pMonitor) return;

    if (!pLayer->m_mapped || pLayer->m_readyToDelete || !pLayer->m_layerSurface) return;

    Vector2D oRealPosition = pLayer->m_position;
    Vector2D oSize = pLayer->m_geometry.size();
    float oAlpha = pLayer->m_alpha->value(); // set to 1 to show hidden top layer
    const auto oFadingOut = pLayer->m_fadingOut;

    const float curScaling = rectOverride.w / (oSize.x);

    Render::SRenderModifData renderModif;

    renderModif.modifs.push_back(std::make_pair(Render::SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, std::any(pMonitor->m_position + (rectOverride.pos() / curScaling) - oRealPosition)));
    renderModif.modifs.push_back(std::make_pair(Render::SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, std::any(curScaling)));
    renderModif.enabled = true;
    pLayer->m_alpha->setValue(1);
    pLayer->m_fadingOut = false;

    g_pHyprRenderer->m_renderPass.add(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{.renderModif = renderModif}));
    Hyprutils::Utils::CScopeGuard x([] {
        g_pHyprRenderer->m_renderPass.add(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{.renderModif = Render::SRenderModifData{}}));
    });

    (*(tRenderLayer)pRenderLayer)(g_pHyprRenderer.get(), pLayer, pMonitor, time, false, false);

    pLayer->m_fadingOut = oFadingOut;
    pLayer->m_alpha->setValue(oAlpha);
}

// NOTE: rects and clipbox positions are relative to the monitor, while damagebox and layers are not, what the fuck? xd
void CHyprspaceWidget::draw() {

    workspaceBoxes.clear();

    if (!active && !curYOffset->isBeingAnimated()) return;

    auto owner = getOwner();

    if (!owner) return;

    // Full-monitor clip in monitor-local coords. Never use default CBox() to "clear" clipBox —
    // hyprutils::CBox() only sets w/h to 0 and leaves x/y uninitialized, which corrupts scissor state.
    const CBox monitorClip = {{0, 0}, owner->m_transformedSize};

    const auto time = Time::steadyNow();

    owner->m_blurFBShouldRender = true;

    int bottomInvert = 1;
    if (Config::onBottom) bottomInvert = -1;

    // Background box
    CBox widgetBox = {owner->m_position.x, owner->m_position.y + (Config::onBottom * (owner->m_transformedSize.y - ((Config::panelHeight + Config::reservedArea) * owner->m_scale))) - (bottomInvert * curYOffset->value()), owner->m_transformedSize.x, (Config::panelHeight + Config::reservedArea) * owner->m_scale}; //TODO: update size on monitor change

    // set widgetBox relative to current monitor for rendering panel
    widgetBox.x -= owner->m_position.x;
    widgetBox.y -= owner->m_position.y;

    g_pHyprRenderer->m_renderData.clipBox = monitorClip;

    if (!Config::disableBlur) {
        renderRectWithBlur(widgetBox, Config::panelBaseColor);
    }
    else {
        renderRect(widgetBox, Config::panelBaseColor);
    }

    // Panel Border
    if (Config::panelBorderWidth > 0) {
        // Border box
        CBox borderBox = {widgetBox.x, owner->m_position.y + (Config::onBottom * owner->m_transformedSize.y) + (Config::panelHeight + Config::reservedArea - curYOffset->value() * owner->m_scale) * bottomInvert, owner->m_transformedSize.x, static_cast<double>(Config::panelBorderWidth)};
        borderBox.y -= owner->m_position.y;

        renderRect(borderBox, Config::panelBorderColor);
    }


    // unscaled and relative to owner
    //CBox damageBox = {0, (Config::onBottom * (owner->m_transformedSize.y - ((Config::panelHeight + Config::reservedArea)))) - (bottomInvert * curYOffset->value()), owner->m_transformedSize.x, (Config::panelHeight + Config::reservedArea) * owner->m_scale};

    //owner->addDamage(damageBox);
    g_pHyprRenderer->damageMonitor(owner);

    // damage the entire monitor to ensure full redraw during overview
    g_pHyprRenderer->damageMonitor(owner);

    // the list of workspaces to show
    std::vector<int> workspaces;

    if (Config::showSpecialWorkspace) {
        workspaces.push_back(SPECIAL_WORKSPACE_START);
    }

    // find the lowest and highest workspace id to determine which empty workspaces to insert
    int lowestID = INT_MAX;
    int highestID = 1;
    for (auto& ws : g_pCompositor->getWorkspaces()) {
        if (!ws) continue;
        // normal workspaces start from 1, special workspaces ends on -2
        if (ws->m_id < 1) continue;
        if (ws->m_monitor->m_id == ownerID) {
            workspaces.push_back(ws->m_id);
            if (highestID < ws->m_id) highestID = ws->m_id;
            if (lowestID > ws->m_id) lowestID = ws->m_id;
        }
    }

    // include empty workspaces that are between non-empty ones
    if (Config::showEmptyWorkspace) {
        int wsIDStart = 1;
        int wsIDEnd = highestID;

        // hyprsplit/split-monitor-workspaces compatibility
        if (numWorkspaces > 0) {
            wsIDStart = std::min<int>(numWorkspaces * ownerID + 1, lowestID);
            wsIDEnd = std::max<int>(numWorkspaces * ownerID + 1, highestID); // always show the initial workspace for current monitor
        }

        for (int i = wsIDStart; i <= wsIDEnd; i++) {
            if (i == owner->activeSpecialWorkspaceID()) continue;
            const auto pWorkspace = g_pCompositor->getWorkspaceByID(i);
            if (pWorkspace == nullptr)
                workspaces.push_back(i);
        }
    }

    // add a new empty workspace at last
    if (Config::showNewWorkspace) {
        // get the lowest empty workspce id after the highest id of current workspace
        while (g_pCompositor->getWorkspaceByID(highestID) != nullptr) highestID++;
        workspaces.push_back(highestID);
    }

    std::sort(workspaces.begin(), workspaces.end());

    // render workspace boxes
    int wsCount = workspaces.size();
    double monitorSizeScaleFactor = ((Config::panelHeight - 2 * Config::workspaceMargin) / (owner->m_transformedSize.y)) * owner->m_scale; // scale box with panel height
    double workspaceBoxW = owner->m_transformedSize.x * monitorSizeScaleFactor;
    double workspaceBoxH = owner->m_transformedSize.y * monitorSizeScaleFactor;
    double workspaceGroupWidth = workspaceBoxW * wsCount + (Config::workspaceMargin * owner->m_scale) * (wsCount - 1);
    double curWorkspaceRectOffsetX = Config::centerAligned ? workspaceScrollOffset->value() + (widgetBox.w / 2.) - (workspaceGroupWidth / 2.) : workspaceScrollOffset->value() + Config::workspaceMargin;
    double curWorkspaceRectOffsetY = !Config::onBottom ? (((Config::reservedArea + Config::workspaceMargin) * owner->m_scale) - curYOffset->value()) : (owner->m_transformedSize.y - ((Config::reservedArea + Config::workspaceMargin) * owner->m_scale) - workspaceBoxH + curYOffset->value());
    double workspaceOverflowSize = std::max<double>(((workspaceGroupWidth - widgetBox.w) / 2) + (Config::workspaceMargin * owner->m_scale), 0);

    *workspaceScrollOffset = std::clamp<double>(workspaceScrollOffset->goal(), -workspaceOverflowSize, workspaceOverflowSize);

    if (!(workspaceBoxW > 0 && workspaceBoxH > 0)) return;
    for (auto wsID : workspaces) {
        const auto ws = g_pCompositor->getWorkspaceByID(wsID);
        CBox curWorkspaceBox = {curWorkspaceRectOffsetX, curWorkspaceRectOffsetY, workspaceBoxW, workspaceBoxH};

        // workspace background rect (NOT background layer) and border
        if (ws == owner->m_activeWorkspace) {
            if (Config::workspaceBorderSize >= 1 && Config::workspaceActiveBorder.a > 0) {
                renderBorder(curWorkspaceBox, Config::CGradientValueData(Config::workspaceActiveBorder), Config::workspaceBorderSize);
            }
            if (!Config::disableBlur) {
                renderRectWithBlur(curWorkspaceBox, Config::workspaceActiveBackground); // cant really round it until I find a proper way to clip windows to a rounded rect
            }
            else {
                renderRect(curWorkspaceBox, Config::workspaceActiveBackground);
            }
            if (!Config::drawActiveWorkspace) {
                curWorkspaceRectOffsetX += workspaceBoxW + (Config::workspaceMargin * owner->m_scale);
                continue;
            }
        }
        else {
            if (Config::workspaceBorderSize >= 1 && Config::workspaceInactiveBorder.a > 0) {
                renderBorder(curWorkspaceBox, Config::CGradientValueData(Config::workspaceInactiveBorder), Config::workspaceBorderSize);
            }
            if (!Config::disableBlur) {
                renderRectWithBlur(curWorkspaceBox, Config::workspaceInactiveBackground);
            }
            else {
                renderRect(curWorkspaceBox, Config::workspaceInactiveBackground);
            }
        }

        // background and bottom layers
        if (!Config::hideBackgroundLayers) {
            for (auto& ls : owner->m_layerSurfaceLayers[0]) {
                // Explicitly wrap the math or ensure scale factors match 
                // If monitorSizeScaleFactor is a Vector2D, you may need component access (.x, .y) or explicit conversion
                CBox layerBox = {
                    curWorkspaceBox.pos() + (ls->m_position - owner->m_position) * monitorSizeScaleFactor, 
                    ls->m_geometry.size() * monitorSizeScaleFactor
                };
                g_pHyprRenderer->m_renderData.clipBox = curWorkspaceBox;
                renderLayerStub(ls.lock(), owner, layerBox, time);
                g_pHyprRenderer->m_renderData.clipBox = monitorClip;
            }
            for (auto& ls : owner->m_layerSurfaceLayers[1]) {
                // Explicitly wrap the math or ensure scale factors match 
                // If monitorSizeScaleFactor is a Vector2D, you may need component access (.x, .y) or explicit conversion
                CBox layerBox = {
                    curWorkspaceBox.pos() + (ls->m_position - owner->m_position) * monitorSizeScaleFactor, 
                    ls->m_geometry.size() * monitorSizeScaleFactor
                };

                g_pHyprRenderer->m_renderData.clipBox = curWorkspaceBox;
                renderLayerStub(ls.lock(), owner, layerBox, time);
                g_pHyprRenderer->m_renderData.clipBox = monitorClip;
            }
        }

        // the mini panel to cover the awkward empty space reserved by the panel
        if (owner->m_activeWorkspace == ws && Config::affectStrut) {
            Vector2D panelPos  = {curWorkspaceRectOffsetX, curWorkspaceRectOffsetY};
            Vector2D panelSize = {widgetBox.w * monitorSizeScaleFactor, widgetBox.h * monitorSizeScaleFactor};

            CBox miniPanelBox = {panelPos, panelSize};
            if (Config::onBottom) miniPanelBox = {curWorkspaceRectOffsetX, curWorkspaceRectOffsetY + workspaceBoxH - widgetBox.h * monitorSizeScaleFactor, widgetBox.w * monitorSizeScaleFactor, widgetBox.h * monitorSizeScaleFactor};

            if (!Config::disableBlur) {
                renderRectWithBlur(miniPanelBox, CHyprColor(0, 0, 0, 0));
            }
            else {
                // what
                renderRect(miniPanelBox, CHyprColor(0, 0, 0, 0));
            }

        }

        if (ws != nullptr) {
            // draw tiled windows
            for (auto& w : g_pCompositor->m_windows) {
                if (!w) continue;
                if (w->m_workspace == ws && !w->m_isFloating) {
                    double wX = curWorkspaceRectOffsetX + ((w->m_position.x - owner->m_position.x) * monitorSizeScaleFactor * owner->m_scale);
                    double wY = curWorkspaceRectOffsetY + ((w->m_position.y - owner->m_position.y) * monitorSizeScaleFactor * owner->m_scale);
                    double wW = w->m_size.x * monitorSizeScaleFactor * owner->m_scale;
                    double wH = w->m_size.y * monitorSizeScaleFactor * owner->m_scale;
                    if (!(wW > 0 && wH > 0)) continue;
                    CBox curWindowBox = {wX, wY, wW, wH};
                    g_pHyprRenderer->m_renderData.clipBox = curWorkspaceBox;
                    //g_pHyprOpenGL->renderRectWithBlur(&curWindowBox, CHyprColor(0, 0, 0, 0));
                    renderWindowStub(w, owner, owner->m_activeWorkspace, curWindowBox, time);
                    g_pHyprRenderer->m_renderData.clipBox = monitorClip;
                }
            }
            // draw floating windows
            for (auto& w : g_pCompositor->m_windows) {
                if (!w) continue;
                if (w->m_workspace == ws && w->m_isFloating && ws->getLastFocusedWindow() != w) {
                    double wX = curWorkspaceRectOffsetX + ((w->m_position.x - owner->m_position.x) * monitorSizeScaleFactor * owner->m_scale);
                    double wY = curWorkspaceRectOffsetY + ((w->m_position.y - owner->m_position.y) * monitorSizeScaleFactor * owner->m_scale);
                    double wW = w->m_size.x * monitorSizeScaleFactor * owner->m_scale;
                    double wH = w->m_size.y * monitorSizeScaleFactor * owner->m_scale;
                    if (!(wW > 0 && wH > 0)) continue;
                    CBox curWindowBox = {wX, wY, wW, wH};
                    g_pHyprRenderer->m_renderData.clipBox = curWorkspaceBox;
                    //g_pHyprOpenGL->renderRectWithBlur(&curWindowBox, CHyprColor(0, 0, 0, 0));
                    renderWindowStub(w, owner, owner->m_activeWorkspace, curWindowBox, time);
                    g_pHyprRenderer->m_renderData.clipBox = monitorClip;
                }
            }
            // draw last focused floating window on top
            if (ws->getLastFocusedWindow())
                if (ws->getLastFocusedWindow()->m_isFloating) {
                    const auto w = ws->getLastFocusedWindow();
                    double wX = curWorkspaceRectOffsetX + ((w->m_position.x - owner->m_position.x) * monitorSizeScaleFactor * owner->m_scale);
                    double wY = curWorkspaceRectOffsetY + ((w->m_position.y - owner->m_position.y) * monitorSizeScaleFactor * owner->m_scale);
                    double wW = w->m_size.x * monitorSizeScaleFactor * owner->m_scale;
                    double wH = w->m_size.y * monitorSizeScaleFactor * owner->m_scale;
                    if (!(wW > 0 && wH > 0)) continue;
                    CBox curWindowBox = {wX, wY, wW, wH};
                    g_pHyprRenderer->m_renderData.clipBox = curWorkspaceBox;
                    //g_pHyprOpenGL->renderRectWithBlur(&curWindowBox, CHyprColor(0, 0, 0, 0));
                    renderWindowStub(w, owner, owner->m_activeWorkspace, curWindowBox, time);
                    g_pHyprRenderer->m_renderData.clipBox = monitorClip;
                }
        }

        if (owner->m_activeWorkspace != ws || !Config::hideRealLayers) {
            // this layer is hidden for real workspace when panel is displayed
            if (!Config::hideTopLayers)
                for (auto& ls : owner->m_layerSurfaceLayers[2]) {
                    CBox layerBox = {curWorkspaceBox.pos() + (ls->m_position - owner->m_position) * monitorSizeScaleFactor, ls->m_geometry.size() * monitorSizeScaleFactor};
                    g_pHyprRenderer->m_renderData.clipBox = curWorkspaceBox;
                    renderLayerStub(ls.lock(), owner, layerBox, time);
                    g_pHyprRenderer->m_renderData.clipBox = monitorClip;
                }

            if (!Config::hideOverlayLayers)
                for (auto& ls : owner->m_layerSurfaceLayers[3]) {
                    CBox layerBox = {curWorkspaceBox.pos() + (ls->m_position - owner->m_position) * monitorSizeScaleFactor, ls->m_geometry.size() * monitorSizeScaleFactor};
                    g_pHyprRenderer->m_renderData.clipBox = curWorkspaceBox;
                    renderLayerStub(ls.lock(), owner, layerBox, time);
                    g_pHyprRenderer->m_renderData.clipBox = monitorClip;
                }
        }


        // Resets workspaceBox to scaled absolute coordinates for input detection.
        // While rendering is done in pixel coordinates, input detection is done in
        // scaled coordinates, taking into account monitor scaling.
        // Since the monitor position is already given in scaled coordinates,
        // we only have to scale all relative coordinates, then add them to the
        // monitor position to get a scaled absolute position.
        curWorkspaceBox.scale(1 / owner->m_scale);

        curWorkspaceBox.x += owner->m_position.x;
        curWorkspaceBox.y += owner->m_position.y;
        workspaceBoxes.emplace_back(std::make_tuple(wsID, curWorkspaceBox));

        // set the current position to the next workspace box
        curWorkspaceRectOffsetX += workspaceBoxW + Config::workspaceMargin * owner->m_scale;
    }

    g_pHyprRenderer->m_renderData.clipBox = monitorClip;
}
