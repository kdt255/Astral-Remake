// Astral-Remake - made by kdt255 (https://github.com/kdt255/Astral-Remake)
// Free software: MIT + Commons Clause, selling is not permitted. If you paid for this, you got scammed.
#pragma once
// The Astral menu window: animated title bar with tab icons, category card, panel pages, footer with search,
// plus the watermark and binds overlays. UI-thread only.
#include "astral/config.h"
#include "astral/persist.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <string>
#include <vector>

namespace astral {

enum class Tab : int { Legit, Rage, Visuals, Skins, Misc, Configs, Scripts, Count };

struct UserInfo {
    std::string name = "kdt255";
    std::string plan = "Lifetime";
};

class Menu {
public:
    // Loads fonts and builds the style. Call after the platform/renderer backends are initialized.
    bool Init(float system_dpi);
    // Call once per frame before ImGui::NewFrame(): applies pending DPI/accent changes and recycles textures.
    void PreFrame();
    // Call between ImGui::NewFrame() and ImGui::Render().
    void Frame();
    // Warning: call after the renderer backend shut down and before ImGui::DestroyContext().
    void Shutdown();

    void SetSystemDpi(float dpi);
    void SetTab(Tab tab);
    void SetVisible(bool visible);
    void SetSearch(const char* text);
    [[nodiscard]] bool Visible() const noexcept { return open_; }
    [[nodiscard]] Config& config() noexcept { return cfg_; }
    [[nodiscard]] UserInfo& user() noexcept { return user_; }
    [[nodiscard]] ImVec2 WindowPos() const noexcept { return pos_; }
    [[nodiscard]] bool Placed() const noexcept { return placed_; }

private:
    void Window();
    void TitleBar(ImGuiWindow* window, const ImRect& win);
    void Footer(ImGuiWindow* window, const ImRect& win);
    void ResizeGrip(ImGuiWindow* window, const ImRect& win);
    void Overlays();
    [[nodiscard]] float TargetScale() const;

    void PageLegit(const ImRect& card, const ImRect& area);
    void PageRage(const ImRect& card, const ImRect& area);
    void PageVisuals(const ImRect& card, const ImRect& area);
    void PageSkins(const ImRect& card, const ImRect& area);
    void PageMisc(const ImRect& card, const ImRect& area);
    void PageConfigs(const ImRect& card, const ImRect& area);
    void PageScripts(const ImRect& card, const ImRect& area);

    void RefreshConfigs();
    void RefreshScripts();

    Config cfg_;
    UserInfo user_;
    float system_dpi_ = 1.0f;
    float applied_scale_ = 0.0f;
    int applied_accent_ = -1;
    ImVec4 applied_start_ = ImVec4(-1.0f, -1.0f, -1.0f, -1.0f);
    ImVec4 applied_end_ = ImVec4(-1.0f, -1.0f, -1.0f, -1.0f);

    Tab tab_ = Tab::Misc;
    float tab_t_ = 1.0f;
    bool open_ = true;
    float open_t_ = 0.0f;

    ImVec2 pos_ = ImVec2(0.0f, 0.0f);
    ImVec2 size_ = ImVec2(0.0f, 0.0f);
    bool placed_ = false;
    bool resizing_ = false;

    bool search_open_ = false;
    bool search_focus_ = false;
    float search_t_ = 0.0f;

    // Configs page
    std::vector<persist::FileEntry> configs_;
    int config_sort_ = 0;
    int config_selected_ = -1;
    char config_name_[33] = {};
    std::string config_status_;
    bool config_status_error_ = false;
    double config_status_time_ = -10.0;
    bool delete_armed_ = false;
    bool configs_loaded_ = false;

    // Scripts page
    std::vector<persist::FileEntry> scripts_;
    std::vector<std::string> autoload_;
    std::vector<std::string> preview_;
    int script_selected_ = -1;
    int script_filter_ = 0;
    bool scripts_loaded_ = false;
};

} // namespace astral
