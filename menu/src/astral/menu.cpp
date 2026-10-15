// Astral-Remake - made by kdt255 (https://github.com/kdt255/Astral-Remake)
// Free software: MIT + Commons Clause, selling is not permitted. If you paid for this, you got scammed.
#include "astral/menu.h"

#include "astral/draw.h"
#include "astral/ui.h"
#include "astral/fonts/inter.h"

#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <ctime>

namespace astral {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kScales[] = { 0.0f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };
constexpr Icon kTabIcons[] = { Icon::Mouse, Icon::Crosshair, Icon::Eye, Icon::Palette, Icon::Gear, Icon::List, Icon::Code };
constexpr const char* kTabNames[] = { "Legit", "Rage", "Visuals", "Skins", "Miscellaneous", "Configs", "Scripts" };
static_assert(IM_ARRAYSIZE(kTabIcons) == static_cast<int>(Tab::Count), "one icon per tab");

[[nodiscard]] float S(float v) { return draw::Snap(ui::Px(v)); }

struct StarSeed {
    float x, y, radius, alpha, rotation, speed, phase, spin;
};

// Deterministic star field (same layout every run), drifting slowly to the right and wrapping around.
const std::array<StarSeed, 26>& Stars()
{
    static const std::array<StarSeed, 26> stars = [] {
        std::array<StarSeed, 26> out{};
        std::uint32_t state = 0x9E3779B9u;
        auto next = [&state]() {
            state = state * 1664525u + 1013904223u;
            return static_cast<float>((state >> 8) & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
        };
        for (size_t i = 0; i < out.size(); ++i) {
            StarSeed& s = out[i];
            s.x = (static_cast<float>(i) + next() * 0.8f) / static_cast<float>(out.size());
            s.y = 0.16f + next() * 0.68f;
            s.radius = 3.0f + next() * 2.9f;
            s.alpha = 0.55f + next() * 0.45f;
            s.rotation = next() * kPi * 2.0f;
            s.speed = 5.0f + next() * 9.0f;
            s.phase = next() * kPi * 2.0f;
            s.spin = (next() - 0.5f) * 0.6f;
        }
        return out;
    }();
    return stars;
}

void Ribbon(ImDrawList* dl, const ImRect& bar, float phase, float amplitude, float wavelength, float drift, float t)
{
    constexpr int kSegments = 72;
    ImVec2 pts[kSegments + 1];
    ImVec2 echo[kSegments + 1];
    const float cy = bar.GetCenter().y;
    const float gap = ui::Px(3.0f);
    for (int i = 0; i <= kSegments; ++i) {
        const float x = ImLerp(bar.Min.x - ui::Px(4.0f), bar.Max.x + ui::Px(4.0f), static_cast<float>(i) / kSegments);
        const float a = (x - bar.Min.x) / wavelength * kPi * 2.0f + phase + t * drift;
        const float y = cy + std::sin(a) * amplitude;
        pts[i] = ImVec2(x, y);
        echo[i] = ImVec2(x, y + gap * (0.6f + 0.4f * std::cos(a)));
    }
    dl->AddPolyline(pts, kSegments + 1, draw::Col(ImVec4(1, 1, 1, 0.07f)), ImDrawFlags_None, ui::Px(6.0f));
    dl->AddPolyline(echo, kSegments + 1, draw::Col(ImVec4(1, 1, 1, 0.16f)), ImDrawFlags_None, ui::Px(1.0f));
    dl->AddPolyline(pts, kSegments + 1, draw::Col(ImVec4(1, 1, 1, 0.36f)), ImDrawFlags_None, ui::Px(1.2f));
    for (int i = 1; i < kSegments; i += 2) {
        const float tw = 0.5f + 0.5f * std::sin(t * 2.3f + static_cast<float>(i) * 1.7f + phase);
        const ImVec2 p = ImLerp(pts[i], echo[i], 0.5f);
        dl->AddCircleFilled(p, ui::Px(0.85f), draw::Col(ImVec4(1, 1, 1, 0.22f + 0.4f * tw)), 6);
    }
}

} // namespace

bool Menu::Init(float system_dpi)
{
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    ImFontConfig font_cfg;
    font_cfg.FontDataOwnedByAtlas = false; // the TTF lives in static storage for the whole run (required since 1.92)
    font_cfg.PixelSnapH = true;            // integer advances keep every glyph on the pixel grid at any UI scale
    ui::Context& c = ui::Ctx();
    ImFormatString(font_cfg.Name, IM_ARRAYSIZE(font_cfg.Name), "Inter SemiBold");
    c.fonts.regular = io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(fonts::kInterSemiBold),
                                                     static_cast<int>(fonts::kInterSemiBold_size), c.m.font_body, &font_cfg);
    ImFormatString(font_cfg.Name, IM_ARRAYSIZE(font_cfg.Name), "Inter Bold");
    c.fonts.bold = io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(fonts::kInterBold), static_cast<int>(fonts::kInterBold_size),
                                                  c.m.font_header, &font_cfg);
    if (c.fonts.regular == nullptr || c.fonts.bold == nullptr) return false;
    io.FontDefault = c.fonts.regular;

    system_dpi_ = system_dpi;
    PreFrame();
    return true;
}

void Menu::PreFrame()
{
    ui::Context& c = ui::Ctx();
    c.icons.Collect();
    const float scale = TargetScale();
    if (std::fabs(scale - applied_scale_) > 0.001f) {
        if (placed_ && applied_scale_ > 0.0f) {
            const float k = scale / applied_scale_;
            const ImVec2 center(pos_.x + size_.x * 0.5f, pos_.y + size_.y * 0.5f);
            size_ = ImVec2(size_.x * k, size_.y * k);
            pos_ = ImVec2(center.x - size_.x * 0.5f, center.y - size_.y * 0.5f);
        }
        ui::SetScale(scale);
        applied_scale_ = scale;
    }
    cfg_.misc.accent = ImClamp(cfg_.misc.accent, 0, static_cast<int>(AccentPreset::Count) - 1);
    ImVec4 start = cfg_.misc.accent_start.Resolve(c.time);
    ImVec4 end = cfg_.misc.accent_end.Resolve(c.time);
    start.w = end.w = 1.0f;
    auto same = [](const ImVec4& a, const ImVec4& b) { return a.x == b.x && a.y == b.y && a.z == b.z; };
    const bool custom = cfg_.misc.accent == static_cast<int>(AccentPreset::Custom);
    if (cfg_.misc.accent != applied_accent_ || (custom && (!same(start, applied_start_) || !same(end, applied_end_)))) {
        ui::SetAccent(static_cast<AccentPreset>(cfg_.misc.accent), start, end);
        applied_accent_ = cfg_.misc.accent;
        applied_start_ = start;
        applied_end_ = end;
    }
}

void Menu::Shutdown() { ui::Shutdown(); }

void Menu::SetSystemDpi(float dpi) { system_dpi_ = dpi; }

float Menu::TargetScale() const
{
    const int mode = ImClamp(cfg_.misc.dpi_mode, 0, IM_ARRAYSIZE(kScales) - 1);
    return mode == 0 ? system_dpi_ : kScales[mode];
}

void Menu::SetTab(Tab tab)
{
    if (tab == tab_ || tab == Tab::Count) return;
    tab_ = tab;
    tab_t_ = 0.0f;
}

void Menu::SetVisible(bool visible) { open_ = visible; }

void Menu::SetSearch(const char* text)
{
    ui::Context& c = ui::Ctx();
    ImFormatString(c.search, IM_ARRAYSIZE(c.search), "%s", text != nullptr ? text : "");
    search_open_ = c.search[0] != '\0';
    search_t_ = search_open_ ? 1.0f : 0.0f;
}

void Menu::Frame()
{
    ui::Context& c = ui::Ctx();
    c.animations = cfg_.misc.menu_animations;
    c.show_paths = cfg_.misc.dev_menu_paths;
    ui::BeginFrame();

    const ImGuiIO& io = ImGui::GetIO();
    if (!io.WantTextInput && !c.listening && ImGui::IsKeyPressed(ImGuiKey_Insert, false)) open_ = !open_;
    if (open_ && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_F)) {
        search_open_ = true;
        search_focus_ = true;
    }
    const float dt = ImMin(io.DeltaTime, 1.0f / 15.0f);
    if (c.animations) open_t_ = ImSaturate(open_t_ + (open_ ? dt : -dt) / 0.22f);
    else open_t_ = open_ ? 1.0f : 0.0f;

    Overlays();
    if (open_t_ <= 0.0f) return;
    Window();
}

void Menu::Window()
{
    ui::Context& c = ui::Ctx();
    const Metrics& m = c.m;
    const ImGuiIO& io = ImGui::GetIO();
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const ImVec2 min_size(S(m.window_min_w), S(m.window_min_h));

    if (!placed_) {
        size_ = ImVec2(S(m.window_w), S(m.window_h));
        pos_ = ImVec2(vp->WorkPos.x + (vp->WorkSize.x - size_.x) * 0.5f, vp->WorkPos.y + (vp->WorkSize.y - size_.y) * 0.5f);
        placed_ = true;
    }
    size_ = ImVec2(ImMax(size_.x, min_size.x), ImMax(size_.y, min_size.y));
    pos_.x = ImClamp(pos_.x, vp->WorkPos.x - size_.x + S(120.0f), vp->WorkPos.x + vp->WorkSize.x - S(120.0f));
    pos_.y = ImClamp(pos_.y, vp->WorkPos.y, vp->WorkPos.y + vp->WorkSize.y - S(m.title_h));

    const float e = ui::Ease(open_t_);
    ImGui::SetNextWindowPos(draw::Snap(ImVec2(pos_.x, pos_.y + (1.0f - e) * S(10.0f))), ImGuiCond_Always);
    ImGui::SetNextWindowSize(draw::Snap(size_), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, e);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, ui::Px(m.window_rounding));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1.0f, 1.0f));
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                       ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse;
    if (ImGui::Begin("##astral", nullptr, flags)) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImDrawList* dl = window->DrawList;
        const ImRect win(window->Pos, ImVec2(window->Pos.x + window->Size.x, window->Pos.y + window->Size.y));
        const float r = ui::Px(m.window_rounding);

        draw::Glow(ImGui::GetBackgroundDrawList(), win.Min, win.Max, r, draw::Col(Rgb(0x000000), 0.30f), S(m.window_shadow));
        dl->AddRectFilled(win.Min, win.Max, draw::Col(c.pal.window_bg), r);

        TitleBar(window, win);

        const ImRect content(ImVec2(win.Min.x + S(m.pad_x), win.Min.y + S(m.title_h) + S(m.pad_y)),
                             ImVec2(win.Max.x - S(m.pad_x), win.Max.y - S(m.footer_h) - S(m.gap)));
        if (c.animations) tab_t_ = ImMin(1.0f, tab_t_ + ImMin(io.DeltaTime, 1.0f / 15.0f) / 0.24f);
        else tab_t_ = 1.0f;
        const float te = ui::Ease(tab_t_);
        const float dy = draw::Snap((1.0f - te) * S(8.0f));
        const ImRect card(ImVec2(content.Min.x, content.Min.y + dy), ImVec2(content.Max.x, content.Min.y + dy + S(m.card_h)));
        const ImRect area(ImVec2(content.Min.x, card.Max.y + S(m.gap)), ImVec2(content.Max.x, content.Max.y));

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * te);
        dl->PushClipRect(ImVec2(win.Min.x, win.Min.y + S(m.title_h)), ImVec2(win.Max.x, win.Max.y - S(m.footer_h)), true);
        dl->AddRectFilled(card.Min, card.Max, draw::Col(c.pal.card), ui::Px(m.card_rounding));
        ImGui::PushID(static_cast<int>(tab_));
        c.path_tab = kTabNames[static_cast<int>(tab_)];
        c.path_page = "";
        switch (tab_) {
        case Tab::Legit: PageLegit(card, area); break;
        case Tab::Rage: PageRage(card, area); break;
        case Tab::Visuals: PageVisuals(card, area); break;
        case Tab::Skins: PageSkins(card, area); break;
        case Tab::Misc: PageMisc(card, area); break;
        case Tab::Configs: PageConfigs(card, area); break;
        case Tab::Scripts: PageScripts(card, area); break;
        case Tab::Count: break;
        }
        ImGui::PopID();
        dl->PopClipRect();
        ImGui::PopStyleVar();

        Footer(window, win);
        ResizeGrip(window, win);

        // Close the absolute layout with an item so SetCursorScreenPos() never "extends" the window bounds.
        ImGui::SetCursorScreenPos(win.Min);
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
    }
    ImGui::End();
    ImGui::PopStyleVar(5);
}

void Menu::TitleBar(ImGuiWindow* window, const ImRect& win)
{
    ui::Context& c = ui::Ctx();
    const Metrics& m = c.m;
    ImDrawList* dl = window->DrawList;
    const float th = S(m.title_h);
    const ImRect bar(win.Min, ImVec2(win.Max.x, win.Min.y + th));
    const float r = ui::Px(m.window_rounding);
    const float t = static_cast<float>(c.time);

    draw::RectGradient(dl, bar.Min, bar.Max, draw::Col(c.pal.title_left), draw::Col(c.pal.title_right), r, ImDrawFlags_RoundCornersTop, true);
    draw::RectGradient(dl, bar.Min, bar.Max, draw::Col(ImVec4(1, 1, 1, 0.05f)), draw::Col(ImVec4(0, 0, 0, 0.05f)), r,
                       ImDrawFlags_RoundCornersTop, false);

    const char* title = "Astral";
    const float title_fs = ui::FontPx(m.font_title);
    const float title_x = bar.Min.x + S(m.title_text_x);
    const float title_w = draw::TextWidth(c.fonts.bold, title_fs, title);
    const float first_tab = draw::Snap(bar.Max.x - S(m.tab_last_from_right) - S(m.tab_spacing) * static_cast<float>(static_cast<int>(Tab::Count) - 1));
    const float tabs_left = first_tab - S(m.tab_hit) * 0.5f;

    dl->PushClipRect(bar.Min, bar.Max, true);
    const float amp = th * 0.27f;
    Ribbon(dl, bar, 0.4f, amp, bar.GetWidth() * 1.15f, 0.42f, t);
    Ribbon(dl, bar, 0.4f + kPi * 0.92f, amp * 0.86f, bar.GetWidth() * 1.02f, 0.36f, t);

    const float band_l = title_x + title_w + S(4.0f);
    const float band_r = tabs_left + S(6.0f);
    const float band_w = ImMax(band_r - band_l, 1.0f);
    const float fade = S(22.0f);
    for (const StarSeed& s : Stars()) {
        const float x = std::fmod(s.x * band_w + t * s.speed * ui::Px(1.0f), band_w);
        const float edge = ImSaturate(x / fade) * ImSaturate((band_w - x) / fade);
        const float twinkle = 0.72f + 0.28f * std::sin(t * 1.7f + s.phase);
        const float a = s.alpha * twinkle * edge;
        if (a <= 0.01f) continue;
        const ImVec2 p(band_l + x, bar.Min.y + th * s.y);
        draw::Star(dl, p, ui::Px(s.radius) * (0.92f + 0.08f * twinkle), s.rotation + t * s.spin, draw::Col(ImVec4(1, 1, 1, a)));
    }
    dl->PopClipRect();

    const float cy = bar.GetCenter().y;
    draw::TextCapCentered(dl, c.fonts.bold, title_fs, title_x, cy + 1.0f, draw::Col(ImVec4(0, 0, 0, 0.22f)), title);
    draw::TextCapCentered(dl, c.fonts.bold, title_fs, title_x, cy, draw::Col(c.pal.text), title);

    // Drag handle covers the bar left of the tab icons.
    ImGui::SetCursorScreenPos(bar.Min);
    ImGui::InvisibleButton("##drag", ImVec2(ImMax(1.0f, tabs_left - bar.Min.x), th));
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
        const ImVec2 d = ImGui::GetIO().MouseDelta;
        pos_ = ImVec2(pos_.x + d.x, pos_.y + d.y);
    }

    for (int i = 0; i < static_cast<int>(Tab::Count); ++i) {
        const float cx = first_tab + S(m.tab_spacing) * static_cast<float>(i);
        const ImRect hit(ImVec2(cx - S(m.tab_hit) * 0.5f, bar.Min.y + S(6.0f)), ImVec2(cx + S(m.tab_hit) * 0.5f, bar.Max.y - S(6.0f)));
        ImGui::PushID(i);
        const ImGuiID id = window->GetID("##tab");
        ImGui::PopID();
        ImGui::ItemAdd(hit, id);
        bool hovered = false, held = false;
        if (ImGui::ButtonBehavior(hit, id, &hovered, &held)) SetTab(static_cast<Tab>(i));
        ui::Tooltip(kTabNames[i]);
        const float hot = ui::Animate(id, "hot", hovered ? 1.0f : 0.0f, 18.0f, 0.0f);
        const float sel = ui::Animate(id, "sel", tab_ == static_cast<Tab>(i) ? 1.0f : 0.0f, 14.0f, tab_ == static_cast<Tab>(i) ? 1.0f : 0.0f);
        const float a = ImLerp(ImLerp(0.72f, 0.9f, hot), 1.0f, sel);
        const ImVec2 center(cx, draw::Snap(cy) - S(1.0f));
        c.icons.Draw(dl, kTabIcons[i], ImVec2(center.x, center.y + ui::Px(2.0f)), draw::Col(ImVec4(0, 0, 0, 0.07f)));
        c.icons.Draw(dl, kTabIcons[i], ImVec2(center.x, center.y + ui::Px(1.0f)), draw::Col(ImVec4(0, 0, 0, 0.13f)));
        c.icons.Draw(dl, kTabIcons[i], center, draw::Col(ImVec4(1, 1, 1, a)));
    }
}

void Menu::Footer(ImGuiWindow* window, const ImRect& win)
{
    ui::Context& c = ui::Ctx();
    const Metrics& m = c.m;
    ImDrawList* dl = window->DrawList;
    const float fh = S(m.footer_h);
    const ImRect foot(ImVec2(win.Min.x, win.Max.y - fh), win.Max);
    dl->AddRectFilled(foot.Min, foot.Max, draw::Col(c.pal.card), ui::Px(m.window_rounding), ImDrawFlags_RoundCornersBottom);
    const float cy = foot.GetCenter().y;
    const float fs = ui::FontPx(m.font_body);

    // Avatar + user / plan
    const float ar = S(8.0f);
    const ImVec2 ac(foot.Min.x + S(m.footer_text_x) + ar, draw::Snap(cy));
    draw::RectGradient(dl, ImVec2(ac.x - ar, ac.y - ar), ImVec2(ac.x + ar, ac.y + ar), draw::Col(c.pal.accent_top),
                       draw::Col(c.pal.accent_end), ar, 0, false);
    char initial[2] = { user_.name.empty() ? '?' : static_cast<char>(std::toupper(static_cast<unsigned char>(user_.name[0]))), '\0' };
    const float ifs = ui::FontPx(10.0f);
    const float iw = draw::TextWidth(c.fonts.bold, ifs, initial);
    draw::TextCapCentered(dl, c.fonts.bold, ifs, ac.x - iw * 0.5f, ac.y, draw::Col(c.pal.text), initial);

    float x = ac.x + ar + S(7.0f);
    draw::TextCapCentered(dl, c.fonts.regular, fs, x, cy, draw::Col(c.pal.text), user_.name.c_str());
    x += draw::TextWidth(c.fonts.regular, fs, user_.name.c_str());
    char plan[96];
    ImFormatString(plan, IM_ARRAYSIZE(plan), " / %s", user_.plan.c_str());
    draw::TextCapCentered(dl, c.fonts.regular, fs, x, cy, draw::Col(c.pal.text_dim), plan);

    // Search toggle + field
    const ImVec2 sc(draw::Snap(foot.Max.x - S(21.0f)), draw::Snap(cy));
    const ImRect hit(ImVec2(sc.x - S(11.0f), foot.Min.y), ImVec2(sc.x + S(9.0f), foot.Max.y));
    const ImGuiID id = window->GetID("##search_toggle");
    ImGui::ItemAdd(hit, id);
    bool hovered = false, held = false;
    if (ImGui::ButtonBehavior(hit, id, &hovered, &held)) {
        search_open_ = !search_open_;
        search_focus_ = search_open_;
        if (!search_open_) c.search[0] = '\0';
    }
    ui::Tooltip("Search");
    const float hot = ui::Animate(id, "hot", (hovered || search_open_) ? 1.0f : 0.0f, 18.0f, 0.0f);
    c.icons.Draw(dl, Icon::Search, sc, draw::Col(draw::Mix(c.pal.text_soft, c.pal.text, hot)));

    const float dt = ImMin(ImGui::GetIO().DeltaTime, 1.0f / 15.0f);
    search_t_ = c.animations ? ImSaturate(search_t_ + (search_open_ ? dt : -dt) / 0.18f) : (search_open_ ? 1.0f : 0.0f);
    if (search_t_ > 0.2f) {
        const float e = ui::Ease(search_t_);
        const float h = S(18.0f);
        const float w = draw::Snap(S(200.0f) * e);
        const float right = sc.x - S(14.0f);
        ImGui::SetCursorScreenPos(ImVec2(right - w, draw::Snap(cy - h * 0.5f)));
        ImGui::SetNextItemWidth(w);
        ImGui::PushFont(c.fonts.regular, fs);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * e);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(S(8.0f), ImMax(0.0f, (h - fs) * 0.5f)));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ui::Px(m.frame_rounding));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, c.pal.panel);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, c.pal.panel);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, c.pal.panel);
        if (search_focus_) {
            ImGui::SetKeyboardFocusHere();
            search_focus_ = false;
        }
        ImGui::InputTextWithHint("##search", "Search", c.search, IM_ARRAYSIZE(c.search));
        if (ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            search_open_ = false;
            c.search[0] = '\0';
        }
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(3);
        ImGui::PopFont();
    }
}

void Menu::ResizeGrip(ImGuiWindow* window, const ImRect& win)
{
    ui::Context& c = ui::Ctx();
    const ImGuiIO& io = ImGui::GetIO();
    const float zone = S(13.0f);
    const ImRect hit(ImVec2(win.Max.x - zone, win.Max.y - zone), win.Max);
    const ImGuiID id = window->GetID("##resize");
    ImGui::ItemAdd(hit, id);
    bool hovered = false, held = false;
    ImGui::ButtonBehavior(hit, id, &hovered, &held, ImGuiButtonFlags_PressedOnClick);
    const ImGuiID grab_key = ImHashStr("grab", 0, id);
    if (held) {
        if (!resizing_) {
            c.state.SetFloat(grab_key, win.Max.x - io.MousePos.x);
            c.state.SetFloat(grab_key + 1, win.Max.y - io.MousePos.y);
            resizing_ = true;
        }
        const ImVec2 target(io.MousePos.x + c.state.GetFloat(grab_key, 0.0f) - win.Min.x,
                            io.MousePos.y + c.state.GetFloat(grab_key + 1, 0.0f) - win.Min.y);
        size_ = ImVec2(ImMax(target.x, S(c.m.window_min_w)), ImMax(target.y, S(c.m.window_min_h)));
    } else {
        resizing_ = false;
    }
    if (hovered || held) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);

    const float hot = ui::Animate(id, "hot", (hovered || held) ? 1.0f : 0.0f, 18.0f, 0.0f);
    ImDrawList* dl = window->DrawList;
    const float step = S(3.0f);
    const float dot = ImMax(1.0f, ui::Px(1.0f));
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3 - i; ++j) {
            const ImVec2 p(draw::Snap(win.Max.x - S(4.0f) - step * static_cast<float>(i)), draw::Snap(win.Max.y - S(4.0f) - step * static_cast<float>(j)));
            dl->AddRectFilled(ImVec2(p.x - dot, p.y - dot), p, draw::Col(draw::Mix(c.pal.grip, c.pal.text_soft, hot), 0.6f + 0.4f * hot));
        }
    }
}

void Menu::Overlays()
{
    ui::Context& c = ui::Ctx();
    if (cfg_.misc.binds_list) ui::DrawBindsOverlay(1.0f);
    if (!cfg_.misc.watermark) return;

    char text[160];
    const std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_s(&local, &now);
    ImFormatString(text, IM_ARRAYSIZE(text), "astral  |  %s  |  %.0f fps  |  %02d:%02d", user_.name.c_str(),
                   static_cast<double>(ImGui::GetIO().Framerate), local.tm_hour, local.tm_min);

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float fs = ui::FontPx(c.m.font_body);
    const float tw = draw::TextWidth(c.fonts.regular, fs, text);
    const float h = S(28.0f);
    const ImVec2 max(vp->WorkPos.x + vp->WorkSize.x - S(16.0f), vp->WorkPos.y + S(16.0f) + h);
    const ImVec2 min(max.x - tw - S(28.0f), max.y - h);
    const float r = ui::Px(c.m.window_rounding);
    draw::Glow(dl, min, max, r, draw::Col(Rgb(0x000000), 0.35f), S(14.0f), ImVec2(0.0f, S(4.0f)));
    dl->AddRectFilled(min, max, draw::Col(c.pal.panel), r);
    draw::RectGradient(dl, min, ImVec2(max.x, min.y + S(2.0f)), draw::Col(c.pal.title_left), draw::Col(c.pal.title_right), r,
                       ImDrawFlags_RoundCornersTop, true);
    draw::TextCapCentered(dl, c.fonts.regular, fs, min.x + S(14.0f), (min.y + max.y) * 0.5f + S(1.0f), draw::Col(c.pal.text_soft), text);
}

} // namespace astral
